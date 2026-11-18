#include "modbus/modbus_rtu.h"

#include <algorithm>
#include <cstdarg>
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
    startMs = nowMs();   // C#: _modbusStartTime = DateTime.UtcNow at class load
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
