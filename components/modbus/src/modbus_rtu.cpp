#include "modbus/modbus_rtu.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "app_controller/gate_controller.h"
#include "board/pins.h"
#include "config/config_storage.h"
#include "config/public_settings.h"
#include "config/advanced_settings.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "motor_control/time_util.h"

static const char* TAG = "modbus";

// RS485 port: UART2 (COM3 in the C#), 9600 8N1
static constexpr uart_port_t kUart = UART_NUM_2;
static constexpr int kBaud = 9600;
static constexpr int kRxBufferSize = 512;   // driver RX ring buffer (C# ReadBufferSize)

// Timing
static constexpr int kMsPerByte = 2;        // 9600 8N1: 1.042 ms/byte + margin
static constexpr int kInterFrameMs = 4;     // 3.5 char times at 9600 = 3.65 ms

// Task
static constexpr int kTaskStack = 4096;
static constexpr UBaseType_t kTaskPriority = 5;

ModbusSlave::ModbusSlave(GateController& gate, const Io& io, PublicSettings& pub,
                         AdvancedSettings& adv, ConfigStorage& store)
    : gate(gate), io(io), pub(pub), adv(adv), store(store)
{
    this->startMs = nowMs();   // was _modbusStartTime = DateTime.UtcNow
}

// CRC-16 Modbus: polynomial 0xA001, LSB first.
static uint16_t modbusCrc16(const uint8_t* data, int length)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if ((crc & 0x0001) != 0) {
                crc = static_cast<uint16_t>((crc >> 1) ^ 0xA001);
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// Write the CRC of the first length bytes behind them, low byte first.
static void appendCrc(uint8_t* data, int length)
{
    const uint16_t crc = modbusCrc16(data, length);
    data[length] = static_cast<uint8_t>(crc & 0xFF);
    data[length + 1] = static_cast<uint8_t>(crc >> 8);
}

// True when the last two bytes are the CRC of the bytes before them.
static bool crcValid(const uint8_t* data, int length)
{
    if (length < 4) {
        return false;
    }
    const uint16_t received = static_cast<uint16_t>((data[length - 1] << 8) | data[length - 2]);
    const uint16_t computed = modbusCrc16(data, length - 2);
    return received == computed;
}

// First register or coil number of a request. Official Modbus numbering: the request
// carries number - 1, so wire address 0 is register 1.
static int firstNumber(const uint8_t* req)
{
    return ((req[2] << 8) | req[3]) + 1;
}

// Called at start-up and after the public settings are saved; takes effect at once.
void ModbusSlave::applyAddress()
{
    int addr = pub.getModbusAddress();
    if (addr < 1 || addr > 247) {
        addr = modbus::kDefaultSlaveId;
    }
    if (addr != slaveId) {
        ESP_LOGI(TAG, "Slave address -> %d", addr);
    }
    slaveId = static_cast<uint8_t>(addr);
}

void ModbusSlave::start()
{
    applyAddress();
    ESP_LOGI(TAG, "Init - UART2 GPIO19(TX)/GPIO20(RX) DE=GPIO47");

    uart_config_t cfg = {};
    cfg.baud_rate = kBaud;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;

    // No TX buffer: uart_write_bytes() returns once the bytes are in the hardware FIFO.
    esp_err_t err = uart_driver_install(kUart, kRxBufferSize, 0, 0, nullptr, 0);
    if (err == ESP_OK) {
        err = uart_param_config(kUart, &cfg);
    }
    if (err == ESP_OK) {
        // RS485 half duplex: the UART drives DE (its RTS pin) high while sending, low otherwise.
        // Set before the pins, so DE is low from the start.
        err = uart_set_mode(kUart, UART_MODE_RS485_HALF_DUPLEX);
    }
    if (err == ESP_OK) {
        err = uart_set_pin(kUart, pins::kModbusTx, pins::kModbusRx, pins::kModbusDe, UART_PIN_NO_CHANGE);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init FAILED: %s", esp_err_to_name(err));
        return;
    }
    active = true;

    ESP_LOGI(TAG, "Port open - slave %d  %d 8N1", slaveId.load(), kBaud);
    ESP_LOGI(TAG, "Peripherals FC04 1-22, Coils FC01/FC05 1-9");
    ESP_LOGI(TAG, "Identification FC04 100-111, CLI FC06 200");

    if (xTaskCreate(&ModbusSlave::taskEntry, "modbus", kTaskStack, this, kTaskPriority, &task) != pdPASS) {
        ESP_LOGE(TAG, "Task start FAILED");
        return;
    }
    ESP_LOGI(TAG, "Thread started");
}

void ModbusSlave::taskEntry(void* arg)
{
    static_cast<ModbusSlave*>(arg)->run();
    vTaskDelete(nullptr);   // a FreeRTOS task must not return
}

// MbLog / DrainMbLog: no log queue. ESP_LOG is fast enough to call from the request loop,
// and the per-frame lines are ESP_LOGD, so a normal build does not print them at all.

void ModbusSlave::run()
{
    ESP_LOGI(TAG, "Thread running");

    while (active) {
        processCli();   // was ProcessModbusCli() in the gate loop

        const int rxLen = receiveFrame(rxBuf, kRxBufSize);

        if (rxLen < 4) {
            sleepMs(kInterFrameMs);
            continue;
        }

        if (rxBuf[0] != slaveId) {
            continue;
        }

        if (!crcValid(rxBuf, rxLen)) {
            ESP_LOGW(TAG, "CRC error - frame discarded");
            continue;
        }

        const uint8_t fc = rxBuf[1];
        switch (static_cast<modbus::FunctionCode>(fc)) {
        case modbus::FunctionCode::ReadCoils: handleReadCoils(rxBuf, rxLen); break;
        case modbus::FunctionCode::ReadDiscreteInputs: handleReadDiscreteInputs(rxBuf, rxLen); break;
        case modbus::FunctionCode::ReadHoldingRegisters: handleReadHoldingRegisters(rxBuf, rxLen); break;
        case modbus::FunctionCode::ReadInputRegisters: handleReadInputRegisters(rxBuf, rxLen); break;
        case modbus::FunctionCode::WriteSingleCoil: handleWriteSingleCoil(rxBuf, rxLen); break;
        case modbus::FunctionCode::WriteSingleRegister: handleWriteSingleRegister(rxBuf, rxLen); break;
        default:
            sendException(fc, 0x01);
            ESP_LOGW(TAG, "Unsupported FC: 0x%02X", fc);
            break;
        }
        logFrame(rxBuf, rxLen);   // debug level: off in a normal build
    }

    ESP_LOGI(TAG, "Thread stopped");
}

// Bytes waiting in the UART driver (was SerialPort.BytesToRead).
static int bytesToRead()
{
    size_t avail = 0;
    if (uart_get_buffered_data_len(kUart, &avail) != ESP_OK) {
        return 0;
    }
    return static_cast<int>(avail);
}

int ModbusSlave::receiveFrame(uint8_t* buf, int size)
{
    int len = 0;

    // Wait for first byte - short idle poll
    const int64_t start = nowMs();
    while (bytesToRead() == 0) {
        if (nowMs() - start > 10) {
            return 0;
        }
        sleepMs(2);
    }

    // Accumulate for 40 ms from the first byte (8-byte frame = 8.3 ms on the wire;
    // 40 ms = 5x the nanoFramework jitter margin)
    const int64_t deadline = nowMs() + 40;
    while (nowMs() < deadline) {
        const int avail = bytesToRead();
        if (avail > 0) {
            const int read = uart_read_bytes(kUart, buf + len, std::min(avail, size - len), 0);
            if (read < 0) {
                return 0;   // C#: catch { len = 0; }
            }
            len += read;
            if (len >= size) {
                break;
            }
        }
        sleepMs(2);
    }
    return len;
}

// FC01 Read Coils. Coils 1-9 are momentary command triggers: always 0 between commands.
// Wire address N is coil N + 1.
void ModbusSlave::handleReadCoils(const uint8_t* req, int reqLen)
{
    if (reqLen < 6) {
        sendException(0x01, 0x03);
        return;
    }

    const int start = firstNumber(req);
    const int count = (req[4] << 8) | req[5];

    // Valid range: coils 1-9
    if (count < 1 || count > 9 || start < 1 || start + count > 10) {
        sendException(0x01, 0x02);
        return;
    }

    const int byteCount = (count + 7) / 8;
    respBuf[0] = slaveId;
    respBuf[1] = 0x01;
    respBuf[2] = static_cast<uint8_t>(byteCount);
    // Coils are momentary: always 0 (no persistent state)
    memset(&respBuf[3], 0, byteCount);
    appendCrc(respBuf, 3 + byteCount);
    send(respBuf, 3 + byteCount + 2);
}

// FC02 Read Discrete Inputs: input registers 1-22 as bits (wire 0-21).
void ModbusSlave::handleReadDiscreteInputs(const uint8_t* req, int reqLen)
{
    if (reqLen < 6) {
        sendException(0x02, 0x03);
        return;
    }

    const int start = firstNumber(req);
    const int count = (req[4] << 8) | req[5];

    if (count < 1 || count > 22 || start + count > 23) {
        sendException(0x02, 0x02);
        return;
    }

    const int byteCount = (count + 7) / 8;
    respBuf[0] = slaveId;
    respBuf[1] = 0x02;
    respBuf[2] = static_cast<uint8_t>(byteCount);
    memset(&respBuf[3], 0, byteCount);

    const GateStatus st = gate.getStatus();
    for (int i = 0; i < count; i++) {
        if (readInputRegister(start + i, st) != 0) {
            respBuf[3 + i / 8] |= static_cast<uint8_t>(1 << (i % 8));
        }
    }

    appendCrc(respBuf, 3 + byteCount);
    send(respBuf, 3 + byteCount + 2);
}

// FC03 Read Holding Registers. Modbus allows at most 125 registers per request
// (250 data bytes), so respBuf (255 bytes) fits any valid request.
void ModbusSlave::handleReadHoldingRegisters(const uint8_t* req, int reqLen)
{
    if (reqLen < 6) {
        sendException(0x03, 0x03);
        return;
    }

    const int start = firstNumber(req);
    const int count = (req[4] << 8) | req[5];

    if (count < 1 || count > 125) {
        sendException(0x03, 0x03);
        return;
    }

    respBuf[0] = slaveId;
    respBuf[1] = 0x03;
    respBuf[2] = static_cast<uint8_t>(count * 2);

    for (int i = 0; i < count; i++) {
        const int val = readHoldingRegister(start + i);
        if (val < 0) {
            sendException(0x03, 0x02);   // illegal data address
            return;
        }
        respBuf[3 + i * 2] = static_cast<uint8_t>(val >> 8);
        respBuf[3 + i * 2 + 1] = static_cast<uint8_t>(val & 0xFF);
    }

    appendCrc(respBuf, 3 + count * 2);
    send(respBuf, 3 + count * 2 + 2);
}

// FC04 Read Input Registers; same limits as FC03.
void ModbusSlave::handleReadInputRegisters(const uint8_t* req, int reqLen)
{
    if (reqLen < 6) {
        sendException(0x04, 0x03);
        return;
    }

    const int start = firstNumber(req);
    const int count = (req[4] << 8) | req[5];

    if (count < 1 || count > 125) {
        sendException(0x04, 0x03);
        return;
    }

    respBuf[0] = slaveId;
    respBuf[1] = 0x04;
    respBuf[2] = static_cast<uint8_t>(count * 2);

    const GateStatus st = gate.getStatus();
    for (int i = 0; i < count; i++) {
        const int val = readInputRegister(start + i, st);
        if (val < 0) {
            sendException(0x04, 0x02);   // illegal data address
            return;
        }
        respBuf[3 + i * 2] = static_cast<uint8_t>(val >> 8);
        respBuf[3 + i * 2 + 1] = static_cast<uint8_t>(val & 0xFF);
    }

    appendCrc(respBuf, 3 + count * 2);
    send(respBuf, 3 + count * 2 + 2);
}

// FC05 Write Single Coil. Coils 1-9 simulate momentary button presses:
// 0xFF00 presses, 0x0000 does nothing (release).
void ModbusSlave::handleWriteSingleCoil(const uint8_t* req, int reqLen)
{
    if (reqLen < 6) {
        sendException(0x05, 0x03);
        return;
    }

    const int coil = firstNumber(req);
    const bool on = (req[4] == 0xFF && req[5] == 0x00);

    if (coil < static_cast<int>(modbus::Coil::PushOpen) || coil > static_cast<int>(modbus::Coil::AutoClose)) {
        sendException(0x05, 0x02);
        return;
    }

    if (on) {
        switch (static_cast<modbus::Coil>(coil)) {
        case modbus::Coil::PushOpen:
            gate.post(GateCommand::Open);
            ESP_LOGI(TAG, "Coil 1 -> Push Open");
            break;
        case modbus::Coil::PushClose:
            gate.post(GateCommand::Close);
            ESP_LOGI(TAG, "Coil 2 -> Push Close");
            break;
        case modbus::Coil::PushOsc:
            gate.post(GateCommand::Osc);
            ESP_LOGI(TAG, "Coil 3 -> Push Open Stop Close");
            break;
        case modbus::Coil::Pedestrian:
            gate.post(GateCommand::Pedestrian);
            ESP_LOGI(TAG, "Coil 4 -> Pedestrian");
            break;
        case modbus::Coil::AutoClose:
            gate.post(GateCommand::ToggleAutoClose);
            ESP_LOGI(TAG, "Coil 9 -> Autoclose");
            break;
        default:
            ESP_LOGD(TAG, "Coil %d reserved - ignored", coil);
            break;
        }
    }

    // Echo request as response (Modbus FC05 standard)
    send(req, reqLen);
}


// FC06 Write Single Register: 100 test register, 120-126 public settings,
// 200 CLI command (1-9) or 0 to acknowledge.
void ModbusSlave::handleWriteSingleRegister(const uint8_t* req, int reqLen)
{
    if (reqLen < 6) {
        sendException(0x06, 0x03);
        return;
    }

    const int addr = firstNumber(req);
    const uint16_t val = static_cast<uint16_t>((req[4] << 8) | req[5]);
    const auto reg = static_cast<modbus::Register>(addr);

    if (reg == modbus::Register::TestRegister) {
        testRegister = val;
        ESP_LOGD(TAG, "TestReg[100] <- 0x%04X  (FC04 reads 0x%04X)", val, static_cast<uint16_t>(~val));
        send(req, reqLen);
        return;
    }

    // Public settings, same units as the FC04 read:
    //   120 AutoCloseDelay s, 121 PedestrianModeDistance cm (stored as m), 122 PassageLightDelay s,
    //   123 BeaconPreWarningTime s, 124 reserved (echo only), 125 StopCount, 126 reserved (echo only)
    if (reg >= modbus::Register::AutoCloseDelay && reg <= modbus::Register::GatePassageMeters) {
        bool publicChanged = false;

        switch (reg) {
        case modbus::Register::AutoCloseDelay:
            pub.setAutoCloseDelay(val);
            publicChanged = true;
            ESP_LOGI(TAG, "AutoCloseDelay <- %ds", val);
            break;
        case modbus::Register::PedestrianDistance:
            // Stored as float metres; Modbus unit is centimetres
            pub.setPedestrianModeDistance(val / 100.0f);
            publicChanged = true;
            ESP_LOGI(TAG, "PedestrianModeDistance <- %dcm (%.2fm)", val, val / 100.0);
            break;
        case modbus::Register::PassageLightDelay:
            pub.setPassageLightDelay(val);
            publicChanged = true;
            ESP_LOGI(TAG, "PassageLightDelay <- %ds", val);
            break;
        case modbus::Register::BeaconPreWarningTime:
            pub.setBeaconPreWarningTime(val);
            publicChanged = true;
            ESP_LOGI(TAG, "BeaconPreWaringTime <- %ds", val);
            break;
        case modbus::Register::AutoRedirectEnabled:
            ESP_LOGD(TAG, "Addr 124 reserved - echo only");
            break;
        case modbus::Register::StopCount:
            adv.setStopCount(val);   // no flash write - takes too long
            ESP_LOGI(TAG, "StopCount <- %d (RAM only)", val);
            break;
        case modbus::Register::GatePassageMeters:
            ESP_LOGD(TAG, "Addr 126 reserved - echo only");
            break;
        default:
            break;
        }

        // Persist to flash if a setting was changed
        if (publicChanged) {
            store.savePublic(pub);
        }

        send(req, reqLen);
        return;
    }

    if (reg == modbus::Register::Cli) {
        if (val == static_cast<uint16_t>(modbus::CliCode::None)) {
            cliStatus = modbus::CliStatus::Idle;
            cliCmd = static_cast<uint8_t>(modbus::CliCode::None);
            ESP_LOGI(TAG, "CLI reg 200 cleared (idle)");
        } else if (val >= static_cast<uint16_t>(modbus::CliCode::Status) && val <= static_cast<uint16_t>(modbus::CliCode::Reset)) {
            if (cliStatus == modbus::CliStatus::Pending) {
                ESP_LOGW(TAG, "CLI busy - command %d rejected", val);
                sendException(0x06, 0x04);
                return;
            }
            cliCmd = static_cast<uint8_t>(val);
            cliStatus = modbus::CliStatus::Pending;
            cliRequested = true;
            ESP_LOGI(TAG, "CLI [%d] queued -> '%s'", val, cliCommandName(cliCmd));
        } else {
            sendException(0x06, 0x03);
            return;
        }

        send(req, reqLen);
        return;
    }

    sendException(0x06, 0x02);
}

// One readable line for every CRC-correct request addressed to this slave, e.g.
//   [MB RX] FC04 InputReg     addr=1 cnt=22
//   [MB RX] FC05 Coil=3 ON  (Push Open Stop Close)
//   [MB RX] FC06 addr=200 val=0x0008 CLI-8(version)
void ModbusSlave::logFrame(const uint8_t* buf, int len)
{
    if (len < 4) {
        return;
    }

    const uint8_t fc = buf[1];
    const int addr = (len >= 6) ? firstNumber(buf) : -1;   // the number, not the wire address
    const int param = (len >= 6) ? ((buf[4] << 8) | buf[5]) : -1;

    switch (static_cast<modbus::FunctionCode>(fc)) {
    case modbus::FunctionCode::ReadCoils:
        ESP_LOGD(TAG, "[MB RX] FC01 Coils        addr=%d cnt=%d", addr, param);
        break;
    case modbus::FunctionCode::ReadDiscreteInputs:
        ESP_LOGD(TAG, "[MB RX] FC02 DiscreteIn   addr=%d cnt=%d", addr, param);
        break;
    case modbus::FunctionCode::ReadHoldingRegisters:
        ESP_LOGD(TAG, "[MB RX] FC03 Holding      addr=%d cnt=%d", addr, param);
        break;
    case modbus::FunctionCode::ReadInputRegisters:
        ESP_LOGD(TAG, "[MB RX] FC04 InputReg     addr=%d cnt=%d", addr, param);
        break;
    case modbus::FunctionCode::WriteSingleCoil: {
        const bool on = (param == 0xFF00);
        ESP_LOGD(TAG, "[MB RX] FC05 Coil=%d %s (%s)", addr, on ? "ON " : "OFF", coilName(addr));
        break;
    }
    case modbus::FunctionCode::WriteSingleRegister: {
        char detail[40] = "";
        const auto reg = static_cast<modbus::Register>(addr);
        if (reg == modbus::Register::TestRegister) {
            snprintf(detail, sizeof(detail), "TestReg");
        } else if (reg == modbus::Register::Cli && param == static_cast<int>(modbus::CliCode::None)) {
            snprintf(detail, sizeof(detail), "CLI-clear");
        } else if (reg == modbus::Register::Cli) {
            snprintf(detail, sizeof(detail), "CLI-%d(%s)", param, cliCommandName(static_cast<uint8_t>(param)));
        }
        ESP_LOGD(TAG, "[MB RX] FC06 addr=%d val=0x%04X %s", addr, param, detail);
        break;
    }
    default:
        ESP_LOGD(TAG, "[MB RX] FC%02X UNKNOWN addr=%d param=%d", fc, addr, param);
        break;
    }
}

const char* ModbusSlave::coilName(int coil)
{
    const auto c = static_cast<modbus::Coil>(coil);
    switch (c) {
    case modbus::Coil::PushOpen: return "Push Open";
    case modbus::Coil::PushClose: return "Push Close";
    case modbus::Coil::PushOsc: return "Push Open Stop Close";
    case modbus::Coil::Pedestrian: return "Pedestrian";
    case modbus::Coil::AutoClose: return "Autoclose";
    default: return c >= modbus::Coil::ReservedFirst && c <= modbus::Coil::ReservedLast ? "reserved" : "?";
    }
}

const char* ModbusSlave::cliCommandName(uint8_t code)
{
    switch (static_cast<modbus::CliCode>(code)) {
    case modbus::CliCode::Status: return "status";
    case modbus::CliCode::Gpio: return "gpio";
    case modbus::CliCode::Errors: return "errors";
    case modbus::CliCode::Clear: return "clear";
    case modbus::CliCode::Counters: return "counters";
    case modbus::CliCode::Cell: return "cell";
    case modbus::CliCode::Mqtt: return "mqtt";
    case modbus::CliCode::Version: return "version";
    case modbus::CliCode::Reset: return "reset";
    default: return "unknown";
    }
}

// RS485 transmit. The UART raises DE while it sends (RS485 half-duplex mode).
void ModbusSlave::send(const uint8_t* data, int length)
{
    if (!active) {
        return;
    }

    // The C# held DE this long (wire time + margin). The UART now drops DE itself after the
    // last stop bit; the wait stays so the turnaround timing does not change.
    const int holdMs = length * kMsPerByte + 2;

    if (uart_write_bytes(kUart, data, length) < 0) {
        ESP_LOGW(TAG, "UART write failed");
    }
    sleepMs(holdMs);

    sleepMs(kInterFrameMs);
    uart_flush_input(kUart);   // drop anything received meanwhile
}

void ModbusSlave::sendException(uint8_t fc, uint8_t exceptionCode)
{
    uint8_t resp[5];
    resp[0] = slaveId;
    resp[1] = static_cast<uint8_t>(fc | 0x80);
    resp[2] = exceptionCode;
    appendCrc(resp, 3);
    send(resp, 5);
}
