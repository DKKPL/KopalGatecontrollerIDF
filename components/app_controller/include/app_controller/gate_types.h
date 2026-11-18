#ifndef APP_CONTROLLER_GATE_TYPES_H
#define APP_CONTROLLER_GATE_TYPES_H

#include <cstdint>

/**
 * @brief The step the gate state machine is in.
 */
enum class MainLoopStep : uint8_t {
    Teach,                        ///< learning the travel, or finding the position after power-up
    Closing,                      ///< motor running towards closed
    ClosingNormalStop,            ///< stopped after closing (fully closed or stopped halfway)
    Opening,                      ///< motor running towards open
    OpeningNormalStop,            ///< stopped after opening (fully open or stopped halfway)
    SafetyCloseAfterClosing,      ///< photocell or torque limit tripped while closing
    SafetyOpenCloseAfterClosing,  ///< safety edge pressed while closing
    SafetyOpenAfterOpening,       ///< torque limit tripped while opening
    SafetyOpenCloseAfterOpening,  ///< safety edge pressed while opening
};

/**
 * @brief The step's name as text, for log messages.
 *        With inline we do not need that extra .cpp file just for this function.
 */
inline const char* toString(MainLoopStep step)
{
    switch (step) {
    case MainLoopStep::Teach:                       return "Teach";
    case MainLoopStep::Closing:                     return "Closing";
    case MainLoopStep::ClosingNormalStop:           return "ClosingNormalStop";
    case MainLoopStep::Opening:                     return "Opening";
    case MainLoopStep::OpeningNormalStop:           return "OpeningNormalStop";
    case MainLoopStep::SafetyCloseAfterClosing:     return "SafetyCloseAfterClosing";
    case MainLoopStep::SafetyOpenCloseAfterClosing: return "SafetyOpenCloseAfterClosing";
    case MainLoopStep::SafetyOpenAfterOpening:      return "SafetyOpenAfterOpening";
    case MainLoopStep::SafetyOpenCloseAfterOpening: return "SafetyOpenCloseAfterOpening";
    }
    return "?";
}

/**
 * @brief The step of the teach run.
 */
enum class TeachPhase : uint8_t {
    CloseToLimit,   ///< closing until the close limit switch
    OpenToLimit,    ///< opening until the open limit switch, counting pulses
    SavePosition,   ///< storing the open position
    Done,           ///< teach finished
};

/**
 * @brief The teach step's name as text, for log messages.
 */
inline const char* toString(TeachPhase phase)
{
    switch (phase) {
    case TeachPhase::CloseToLimit: return "CloseToLimit";
    case TeachPhase::OpenToLimit:  return "OpenToLimit";
    case TeachPhase::SavePosition: return "SavePosition";
    case TeachPhase::Done:         return "Done";
    }
    return "?";
}

/**
 * @brief Commands other tasks send to the gate (Modbus coils 1-4 and 9, web buttons).
 */
enum class GateCommand : uint8_t {
    Open,             ///< _remoteCmdOpen       -> latched Open
    Close,            ///< _remoteCmdClose      -> latched Close
    Osc,              ///< _remoteCmdPosc       -> latched OSC
    Pedestrian,       ///< _remoteCmdPedestrian -> latched Pedestrian
    ToggleAutoClose,  ///< _remoteCmdAutoclose  -> toggle auto-close
    ApplySettings,    ///< web saved the settings: re-apply USE_MOTOR_DISABLE (was Program.ApplyUseMotorDisableSetting)
};

/**
 * @brief Snapshot of the gate, refreshed by the gate task every loop.
 */
struct GateStatus {
    MainLoopStep step = MainLoopStep::Teach;
    TeachPhase teachPhase = TeachPhase::CloseToLimit;
    int64_t position = 0;          ///< counterTotal, pulses (0 = closed)
    float rpm = 0.0f;
    float speedDuty = 0.0f;
    float torqueDuty = 0.0f;
    bool moving = false;
    bool locked = true;
    bool flash = false;
    bool passageLight = false;
    bool autoClose = false;
    bool pedestrianMode = false;
    bool completeOpen = false;
    bool completeClose = false;
    bool photocellTriggered = false;
    bool safetyEdgeTriggered = false;
    bool torqueLimitTriggered = false;
    bool actualDirectionOpening = false;   ///< bActualMotorDirection (true = opening), for Modbus
    bool lsoActivated = false;             ///< bLsoActivated, for Modbus
    bool lscActivated = false;             ///< bLscActivated, for Modbus
};

#endif // APP_CONTROLLER_GATE_TYPES_H
