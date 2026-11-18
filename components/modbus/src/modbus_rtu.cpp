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
