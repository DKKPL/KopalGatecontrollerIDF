#ifndef APP_CONTROLLER_GATE_TYPES_H
#define APP_CONTROLLER_GATE_TYPES_H

#include <cstdint>

enum class MainLoopStep : uint8_t {
    Teach,
    Closing,
    ClosingNormalStop,
    Opening,
    OpeningNormalStop,
    SafetyCloseAfterClosing,
    SafetyOpenCloseAfterClosing,
    SafetyOpenAfterOpening,
    SafetyOpenCloseAfterOpening,
};

// With inline we do not need that extra .cpp file just for this function.
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

enum class TeachPhase : uint8_t {
    CloseToLimit,
    OpenToLimit,
    SavePosition,
    Done,
};

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

// Commands other tasks send to the gate (Modbus coils 1-4 and 9, web buttons).
enum class GateCommand : uint8_t {
    Open,             // _remoteCmdOpen       -> latched Open
    Close,            // _remoteCmdClose      -> latched Close
    Osc,              // _remoteCmdPosc       -> latched OSC
    Pedestrian,       // _remoteCmdPedestrian -> latched Pedestrian
    ToggleAutoClose,  // _remoteCmdAutoclose  -> toggle auto-close
};

// Snapshot of the gate, refreshed by the gate task every loop.
struct GateStatus {
    MainLoopStep step = MainLoopStep::Teach;
    TeachPhase teachPhase = TeachPhase::CloseToLimit;
    int64_t position = 0;          // counterTotal, pulses (0 = closed)
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
};

#endif // APP_CONTROLLER_GATE_TYPES_H
