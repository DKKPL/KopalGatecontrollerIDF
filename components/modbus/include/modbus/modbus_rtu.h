#ifndef MODBUS_MODBUS_RTU_H
#define MODBUS_MODBUS_RTU_H

#include <atomic>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "modbus/modbus_registers.h"

class GateController;
class Io;
class PublicSettings;
class AdvancedSettings;
class ConfigStorage;
struct GateStatus;

/**
 * @brief Modbus RTU slave on the RS485 port; runs in its own FreeRTOS task (was Program.Modbus.cs).
 */
class ModbusSlave {
public:
    /**
     * @brief Gate, inputs and settings are created in main.cpp and must outlive the slave.
     */
    ModbusSlave(GateController& gate, const Io& io, PublicSettings& pub,
                AdvancedSettings& adv, ConfigStorage& store);

    /**
     * @brief Open the RS485 port and start the Modbus task; call once at boot, after gate.start()
     *        (was InitAndStartModbusThread).
     */
    void start();
    /**
     * @brief Take the slave address from the public settings, 55 when not 1-247 (was ApplyModbusAddress).
     *        Call again after the public settings are saved; safe from any task.
     */
    void applyAddress();

private:
    /**
     * @brief Largest response: 3 header + 125 * 2 data + 2 CRC bytes.
     */
    static constexpr int kRespBufSize = 255;
    /**
     * @brief Size of the request buffer.
     */
    static constexpr int kRxBufSize = 256;

    /**
     * @brief FreeRTOS entry point; calls run() on the slave passed as arg.
     */
    static void taskEntry(void* arg);
    /**
     * @brief The Modbus task: receive a request, check address and CRC, answer it (was ModbusThreadWorker).
     */
    void run();
    /**
     * @brief Collect one request: wait up to 10 ms for a first byte, then read for 40 ms
     *        (was ModbusReceiveFrame).
     * @return number of bytes in buf, 0 when nothing came
     */
    int receiveFrame(uint8_t* buf, int size);

    /**
     * @brief FC01: coils 1-9 always read 0 (was ModbusHandleReadCoils).
     */
    void handleReadCoils(const uint8_t* req, int reqLen);
    /**
     * @brief FC02: input registers 1-22 as bits (was ModbusHandleReadDiscreteInputs).
     */
    void handleReadDiscreteInputs(const uint8_t* req, int reqLen);
    /**
     * @brief FC03: identification, test register, last CLI code (was ModbusHandleReadHoldingRegisters).
     */
    void handleReadHoldingRegisters(const uint8_t* req, int reqLen);
    /**
     * @brief FC04: LEDs, identification, settings, CLI status (was ModbusHandleReadInputRegisters).
     */
    void handleReadInputRegisters(const uint8_t* req, int reqLen);
    /**
     * @brief FC05: coils 1-4 and 9 press a button on the gate (was ModbusHandleWriteSingleCoil).
     */
    void handleWriteSingleCoil(const uint8_t* req, int reqLen);
    /**
     * @brief FC06: test register, public settings, CLI command (was ModbusHandleWriteSingleRegister).
     */
    void handleWriteSingleRegister(const uint8_t* req, int reqLen);

    /**
     * @brief Value of one FC04 input register (was ReadInputRegister).
     * @param st gate snapshot, taken once per request
     * @return 0-65535, or -1 when the register is not in the mapping
     */
    int readInputRegister(int addr, const GateStatus& st);
    /**
     * @brief Value of one FC03 holding register (was ReadHoldingRegister).
     * @return 0-65535, or -1 when the register is not in the mapping
     */
    int readHoldingRegister(int addr);

    /**
     * @brief Run the CLI command queued through register 200, if any (was ProcessModbusCli).
     */
    void processCli();
    /**
     * @brief Run one CLI command; its output is one line in the log (was ModbusCliExecute).
     */
    void cliExecute(modbus::CliCode code);

    /**
     * @brief Send a response with DE high, then listen again (was ModbusSend).
     */
    void send(const uint8_t* data, int length);
    /**
     * @brief Send an exception response: fc | 0x80 and the code (was ModbusSendException).
     */
    void sendException(uint8_t fc, uint8_t exceptionCode);

    /**
     * @brief Log one readable line for a request addressed to us (was ModbusLogFrame).
     */
    void logFrame(const uint8_t* buf, int len);
    /**
     * @brief Name of a coil for the frame log (was CoilName).
     */
    static const char* coilName(int coil);
    /**
     * @brief Name of a CLI code, "unknown" outside 1-9 (was CliCommandName).
     */
    static const char* cliCommandName(uint8_t code);

    // Gate, inputs and settings (owned by main.cpp)
    GateController& gate;
    const Io& io;
    PublicSettings& pub;
    AdvancedSettings& adv;
    ConfigStorage& store;

    // Slave address and registers
    std::atomic<uint8_t> slaveId{modbus::kDefaultSlaveId};  ///< the web task may change it via applyAddress()
    uint16_t testRegister = 0xAAAA;                         ///< FC06 100; FC04 100 reads it inverted

    // CLI over Modbus (register 200)
    bool cliRequested = false;                      ///< a command waits for processCli()
    uint8_t cliCmd = 0;                             ///< last CLI code written, 0 after acknowledge
    modbus::CliStatus cliStatus = modbus::CliStatus::Idle;    ///< what FC04 200 reads
    int64_t startMs = 0;                            ///< for the CLI 'counters' uptime

    // Runtime
    TaskHandle_t task = nullptr;
    bool active = false;                            ///< the port is open
    uint8_t respBuf[kRespBufSize] = {};
    uint8_t rxBuf[kRxBufSize] = {};
};

#endif // MODBUS_MODBUS_RTU_H
