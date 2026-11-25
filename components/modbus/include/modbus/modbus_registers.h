#ifndef MODBUS_MODBUS_REGISTERS_H
#define MODBUS_MODBUS_REGISTERS_H

#include <cstdint>

// Modbus RTU slave: 9600 8N1 on UART2 (RS485), slave address from the public
// settings (default 55).
//
// Official Modbus numbering for every function code: the enums below list the numbers
// (1-based), and the request carries number - 1 (register 1 = wire address 0).
//
// FC05 writes 0xFF00 = momentary button press, 0x0000 does nothing; FC01 always reads 0.
// FC02 reads input registers 1-22 as bits.
// FC03 reads 100 raw, 101-111 like FC04, and 200 as the last CLI code.
// CLI (output goes to the log): FC06 write the code to 200, FC04 poll 200 until it
// reads 2 (done), FC06 write 0 to 200 to acknowledge.

namespace modbus {

/**
 * @brief Slave address used when the setting is outside 1-247 (was MODBUS_DEFAULT_SLAVE_ID).
 */
constexpr int kDefaultSlaveId = 55;   // 0x37

/**
 * @brief Firmware version "major.minor" for registers 102 and 103 (was FW_VERSION).
 */
constexpr const char* kFwVersion = "0.9";

/**
 * @brief What FC04 register 200 reads: the state of the CLI command (was CLI_STATUS_*).
 */
enum class CliStatus : uint16_t {
    Idle = 0,     ///< no command (was CLI_STATUS_IDLE)
    Pending = 1,  ///< command queued or running (was CLI_STATUS_PENDING)
    Done = 2,     ///< command finished; write 0 to acknowledge (was CLI_STATUS_DONE)
};

/**
 * @brief Function codes the slave answers; others get exception 01.
 */
enum class FunctionCode : uint8_t {
    ReadCoils = 0x01,             ///< FC01, always reads 0
    ReadDiscreteInputs = 0x02,    ///< FC02, input registers 1-22 as bits
    ReadHoldingRegisters = 0x03,  ///< FC03
    ReadInputRegisters = 0x04,    ///< FC04
    WriteSingleCoil = 0x05,       ///< FC05, 0xFF00 = button press
    WriteSingleRegister = 0x06,   ///< FC06
};

/**
 * @brief Register addresses for FC03, FC04 and FC06 (1-22 also FC02).
 */
enum class Register : uint16_t {
    // PCB input LEDs, 1 = LED on
    PushOpen = 1,                ///< push button Open
    PushClose = 2,               ///< push button Close
    PushOsc = 3,                 ///< push button Open-Stop-Close
    Pedestrian = 4,              ///< pedestrian input
    LimitOpen = 5,               ///< limit switch open
    LimitClose = 6,              ///< limit switch close
    Photocell = 7,               ///< photocell
    SafetyStrip = 8,             ///< safety strip
    AutoClose = 9,               ///< auto-close input
    Learn = 10,                  ///< learn (reset) button
    AuxIn = 11,                  ///< aux input
    // PCB output LEDs
    PassageLight = 12,           ///< passage light
    Flash = 13,                  ///< beacon
    Lock = 14,                   ///< lock, 1 = unlocked
    AuxOut = 15,                 ///< aux output, always 0
    // PCB motor LEDs
    Pulses = 16,                 ///< encoder pulses, always 0
    Direction = 17,              ///< direction feedback pin
    TorqueLimit = 18,            ///< driver torque alarm
    Torque = 19,                 ///< torque PWM, always 0
    Speed = 20,                  ///< speed PWM, always 0
    DirectionReq = 21,           ///< requested direction, 1 = opening
    OnOff = 22,                  ///< motor running
    // Identification
    TestRegister = 100,          ///< FC06 writes it; FC04 reads ~value & 0xFFFF, FC03 the raw value
    ModbusAddress = 101,         ///< slave address in use
    FwMajor = 102,               ///< firmware version major
    FwMinor = 103,               ///< firmware version minor
    ReservedFirst = 104,         ///< 104-111 reserved (formerly IMEI), read 0
    ReservedLast = 111,          ///< last reserved address
    // Public settings, FC04 reads and FC06 writes the same units
    AutoCloseDelay = 120,        ///< s
    PedestrianDistance = 121,    ///< cm (stored as m)
    PassageLightDelay = 122,     ///< s
    BeaconPreWarningTime = 123,  ///< s
    AutoRedirectEnabled = 124,   ///< not implemented, reads 0, write echo only
    StopCount = 125,             ///< cycles; a write is RAM only
    GatePassageMeters = 126,     ///< not implemented, reads 0, write echo only
    // CLI
    Cli = 200,                   ///< FC04: status (CliStatus), FC03: last code, FC06: run a CliCode, 0 = acknowledge
};

/**
 * @brief Coils for FC05 (wire address N = coil N + 1); 5-8 are reserved.
 */
enum class Coil : uint16_t {
    PushOpen = 1,        ///< press Open
    PushClose = 2,       ///< press Close
    PushOsc = 3,         ///< press Open-Stop-Close
    Pedestrian = 4,      ///< press Pedestrian
    ReservedFirst = 5,   ///< 5-8 reserved, ignored
    ReservedLast = 8,    ///< last reserved coil
    AutoClose = 9,       ///< toggle auto-close
};

/**
 * @brief CLI commands written to register 200; the output goes to the log.
 */
enum class CliCode : uint8_t {
    None = 0,      ///< no command / acknowledge
    Status = 1,    ///< gate state and inputs
    Gpio = 2,      ///< pin levels
    Errors = 3,    ///< error counters
    Clear = 4,     ///< clear the error counters
    Counters = 5,  ///< cycle and frame counters
    Cell = 6,      ///< modem, not available
    Mqtt = 7,      ///< MQTT, not available
    Version = 8,   ///< firmware version and build stamp
    Reset = 9,     ///< reset, not available
};

} // namespace modbus

#endif // MODBUS_MODBUS_REGISTERS_H
