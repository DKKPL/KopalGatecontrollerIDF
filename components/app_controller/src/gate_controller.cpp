#include "app_controller/gate_controller.h"

void GateController::drainCommands()
{
    GateCommand cmd;
    while (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
        if (cmd == GateCommand::ApplySettings) {
            motor.applyUseMotorDisableSetting();   // web saved the settings (was Program.ApplyUseMotorDisableSetting)
            continue;
        }
        commands.applyCommand(cmd);
    }
}
