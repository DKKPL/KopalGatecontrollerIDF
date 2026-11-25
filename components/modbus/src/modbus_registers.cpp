#include "modbus/modbus_registers.h"

#include <cinttypes>
#include <cstdlib>
#include <cstring>

#include "app_controller/gate_controller.h"
#include "config/advanced_settings.h"
#include "config/config_storage.h"
#include "config/public_settings.h"
#include "esp_log.h"
#include "hal_io/hal_io.h"
#include "modbus/modbus_rtu.h"
#include "motor_control/time_util.h"

static const char* TAG = "modbus";

// The CLI prints bools like C# did.
static const char* boolText(bool v)
{
    return v ? "True" : "False";
}

// kFwVersion is "major.minor", e.g. "0.8": register 102 = major, 103 = minor.
static uint16_t parseFwMajor()
{
    const char* v = modbus::kFwVersion;
    const char* dot = strchr(v, '.');
    return (dot != nullptr && dot > v) ? static_cast<uint16_t>(atoi(v)) : 0;
}

static uint16_t parseFwMinor()
{
    const char* v = modbus::kFwVersion;
    const char* dot = strchr(v, '.');
    return dot != nullptr ? static_cast<uint16_t>(atoi(dot + 1)) : 0;
}

// FC04 input register: 1-22 peripherals, 100-126 identification and settings, 200 CLI status.
int ModbusSlave::readInputRegister(int addr, const GateStatus& st)
{
    using modbus::Register;
    const auto reg = static_cast<Register>(addr);
    if (reg >= Register::ReservedFirst && reg <= Register::ReservedLast) {
        return 0x0000;   // reserved (formerly IMEI)
    }

    // A bool returns as 1 or 0.
    switch (reg) {
    // PCB input LEDs: read the pin now, 1 = LED on.
    // Active-low (LED on when the pin is low): all except the safety strip.
    case Register::PushOpen: return !io.isHigh(Input::Open);              // GPIO10
    case Register::PushClose: return !io.isHigh(Input::Close);            // GPIO12
    case Register::PushOsc: return !io.isHigh(Input::Osc);                // GPIO11
    case Register::Pedestrian: return !io.isHigh(Input::Pedestrian);      // GPIO13
    case Register::LimitOpen: return !io.isHigh(Input::Lso);              // GPIO8
    case Register::LimitClose: return !io.isHigh(Input::Lsc);             // GPIO9
    case Register::Photocell: return !io.isHigh(Input::Photocell);        // GPIO48
    case Register::SafetyStrip: return io.isHigh(Input::SafetyEdge);      // GPIO1, active-high
    case Register::AutoClose: return !io.isHigh(Input::AutoClose);        // GPIO14
    case Register::Learn: return !io.isHigh(Input::Reset);                // GPIO2
    case Register::AuxIn: return !io.isHigh(Input::AuxIn);                // GPIO35

    // PCB output LEDs. Reading an output pin back was unreliable on nanoFramework
    // (it reads the pad, not the latch), so the gate's own state is used.
    case Register::PassageLight: return st.passageLight;   // GPIO4
    case Register::Flash: return st.flash;                 // GPIO5
    case Register::Lock: return !st.locked;                // GPIO7, inverted: unlocked = LED on
    case Register::AuxOut: return 0x0000;                  // GPIO6, not used

    // PCB motor LEDs. Pulses (GPIO36), torque PWM (GPIO42) and speed PWM (GPIO38)
    // have no pin to read and stay 0.
    case Register::Pulses: return 0x0000;
    case Register::Direction: return io.hasFeedbackDirection() && io.isHigh(Input::FeedbackDirection);  // GPIO37
    case Register::TorqueLimit: return !io.isHigh(Input::TorqueLimit);   // GPIO34, inverted: low = LED on
    case Register::Torque: return 0x0000;
    case Register::Speed: return 0x0000;
    case Register::DirectionReq: return st.actualDirectionOpening;       // GPIO40
    case Register::OnOff: return st.moving;                              // GPIO41

    // Identification
    case Register::TestRegister: return static_cast<uint16_t>(~testRegister & 0xFFFF);
    case Register::ModbusAddress: return slaveId;
    case Register::FwMajor: return parseFwMajor();
    case Register::FwMinor: return parseFwMinor();

    // Public settings, same units as the FC06 write
    case Register::AutoCloseDelay: return static_cast<uint16_t>(pub.getAutoCloseDelay());
    case Register::PedestrianDistance: return static_cast<uint16_t>(pub.getPedestrianModeDistance() * 100 + 0.5f);   // m -> cm
    case Register::PassageLightDelay: return static_cast<uint16_t>(pub.getPassageLightDelay());
    case Register::BeaconPreWarningTime: return static_cast<uint16_t>(pub.getBeaconPreWarningTime());
    case Register::AutoRedirectEnabled: return 0x0000;   // not implemented
    case Register::StopCount: return static_cast<uint16_t>(adv.getStopCount());
    case Register::GatePassageMeters: return 0x0000;     // not implemented

    // CLI status
    case Register::Cli: return static_cast<uint16_t>(cliStatus);

    default: return -1;   // not in the mapping: the handler answers exception 02
    }
}

// FC03 holding register: reads back what FC06 writes (100 raw, 120-126, 200 last CLI code),
// plus the identification 101-111.
int ModbusSlave::readHoldingRegister(int addr)
{
    using modbus::Register;
    const auto reg = static_cast<Register>(addr);
    if (reg >= Register::ReservedFirst && reg <= Register::ReservedLast) {
        return 0x0000;   // reserved (formerly IMEI)
    }

    switch (reg) {
    case Register::TestRegister: return testRegister;   // raw test register
    case Register::ModbusAddress: return slaveId;
    case Register::FwMajor: return parseFwMajor();
    case Register::FwMinor: return parseFwMinor();

    // Public settings, same values and units as FC04
    case Register::AutoCloseDelay: return static_cast<uint16_t>(pub.getAutoCloseDelay());
    case Register::PedestrianDistance: return static_cast<uint16_t>(pub.getPedestrianModeDistance() * 100 + 0.5f);   // m -> cm
    case Register::PassageLightDelay: return static_cast<uint16_t>(pub.getPassageLightDelay());
    case Register::BeaconPreWarningTime: return static_cast<uint16_t>(pub.getBeaconPreWarningTime());
    case Register::AutoRedirectEnabled: return 0x0000;   // not implemented
    case Register::StopCount: return static_cast<uint16_t>(adv.getStopCount());
    case Register::GatePassageMeters: return 0x0000;     // not implemented

    case Register::Cli: return cliCmd;
    default: return -1;   // not in the mapping: the handler answers exception 02
    }
}

// Runs the CLI command queued through register 200 (FC06). Output goes to the log.
void ModbusSlave::processCli()
{
    if (!cliRequested) {
        return;
    }
    cliRequested = false;

    const auto code = static_cast<modbus::CliCode>(cliCmd);
    if (code < modbus::CliCode::Status || code > modbus::CliCode::Reset) {
        ESP_LOGW(TAG, "Unknown CLI code: %d (valid: 1-9)", cliCmd);
        cliStatus = modbus::CliStatus::Idle;
        return;
    }

    ESP_LOGI(TAG, "CLI [%d] -> '%s'", cliCmd, cliCommandName(cliCmd));
    cliStatus = modbus::CliStatus::Pending;
    cliExecute(code);
    cliStatus = modbus::CliStatus::Done;
    ESP_LOGI(TAG, "CLI [%d] done - write 0 to reg 200 to clear", cliCmd);
}

// One short line of output per command, through the log.
void ModbusSlave::cliExecute(modbus::CliCode code)
{
    switch (code) {
    case modbus::CliCode::Status: {
        const GateStatus st = gate.getStatus();
        ESP_LOGI(TAG, "[CLI] moving=%s dir=%s locked=%s flash=%s light=%s autoclose=%s "
                 "lso=%s lsc=%s photo=%s safety=%s torqueLim=%s",
                 boolText(st.moving), boolText(st.actualDirectionOpening), boolText(st.locked),
                 boolText(st.flash), boolText(st.passageLight), boolText(st.autoClose),
                 boolText(st.lsoActivated), boolText(st.lscActivated), boolText(st.photocellTriggered),
                 boolText(st.safetyEdgeTriggered), boolText(st.torqueLimitTriggered));
        break;
    }
    case modbus::CliCode::Gpio: {
        const GateStatus st = gate.getStatus();
        char bits[23];
        for (int i = 1; i <= 22; i++) {
            bits[i - 1] = readInputRegister(i, st) != 0 ? '1' : '0';
        }
        bits[22] = '\0';
        ESP_LOGI(TAG, "[CLI] Peripherals 1-22: %s", bits);
        break;
    }
    case modbus::CliCode::Errors:
        ESP_LOGI(TAG, "[CLI] No error log available");
        break;
    case modbus::CliCode::Clear:
        ESP_LOGI(TAG, "[CLI] Error log cleared");
        break;
    case modbus::CliCode::Counters: {
        const GateStatus st = gate.getStatus();
        const int upSeconds = static_cast<int>((nowMs() - startMs) / 1000);
        ESP_LOGI(TAG, "[CLI] pulses=%" PRId64 " uptime=%ds", st.position, upSeconds);
        break;
    }
    case modbus::CliCode::Version:
        ESP_LOGI(TAG, "[CLI] FW:%s Build:%s", modbus::kFwVersion, ConfigStorage::getFirmwareBuildStamp());
        break;
    case modbus::CliCode::Cell:
    case modbus::CliCode::Mqtt:
    case modbus::CliCode::Reset:
        ESP_LOGI(TAG, "[CLI] '%s' not available", cliCommandName(static_cast<uint8_t>(code)));
        break;
    case modbus::CliCode::None:
        break;
    }
}
