#include "app_controller/gate_commands.h"

#include "esp_log.h"
#include "hal_io/hal_io.h"

static const char* TAG = "commands";

GateCommands::GateCommands(const Io& io) : io(io)
{
}

void GateCommands::pollOsc()
{
    const bool high = io.isHigh(Input::Osc);
    if (!openPressed) {
        // Latch on falling edge.
        if (lastOscHigh && !high) {
            latchedOsc = true;
            ESP_LOGI(TAG, "OSC latched");
        }
    }
    lastOscHigh = high;
}

// Active low. Open wins over close.
bool GateCommands::pollClose()
{
    return closePressed = !io.isHigh(Input::Close) && !openPressed;
}

// Active low.
bool GateCommands::pollOpen()
{
    return openPressed = !io.isHigh(Input::Open);
}

// Active low: pressed = Low, released = High.
void GateCommands::pollPedestrian()
{
    const bool high = io.isHigh(Input::Pedestrian);
    if (!openPressed) {
        if (lastPedestrianHigh && !high) {          // falling edge: pressed
            pedestrianPressed = true;
            pedestrianHold = true;
        } else if (!lastPedestrianHigh && high) {   // rising edge: released
            pedestrianHold = false;
        }
    }
    // If open is pressed, pedestrian is ignored (only the last level is tracked).
    lastPedestrianHigh = high;
}

// Active low.
bool GateCommands::pollAutoClose()
{
    return autoClose = !io.isHigh(Input::AutoClose);
}

void GateCommands::applyCommand(GateCommand cmd)
{
    switch (cmd) {
    case GateCommand::Open:
        latchedOpen = true;
        break;
    case GateCommand::Close:
        latchedClose = true;
        break;
    case GateCommand::Pedestrian:
        latchedPedestrian = true;
        break;
    case GateCommand::Osc:
        latchedOsc = true;
        break;
    case GateCommand::ToggleAutoClose:
        autoClose = !autoClose;
        break;
    case GateCommand::ApplySettings:
        break;   // handled by GateController::drainCommands
    }
}

void GateCommands::clearLatches()
{
    latchedOsc = false;
    latchedOpen = false;
    latchedClose = false;
    latchedPedestrian = false;
}
