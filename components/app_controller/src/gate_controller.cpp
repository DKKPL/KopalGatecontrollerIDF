#include "app_controller/gate_controller.h"

#include <cmath>

#include "config/advanced_settings.h"
#include "config/config_storage.h"
#include "config/public_settings.h"
#include "config/teach_settings.h"
#include "esp_log.h"
#include "hal_counter/hal_counter.h"
#include "hal_io/hal_io.h"
#include "hal_motor/hal_motor.h"
#include "motor_control/time_util.h"

static const char* TAG = "gate";

GateController::GateController(Io& io, Motor& motor, Counter& counter,
                               const PublicSettings& pub, AdvancedSettings& adv,
                               TeachSettings& teach, ConfigStorage& store)
    : io(io), motor(motor), counter(counter), pub(pub), adv(adv), teach(teach), store(store),
      safety(io, adv), safetyActions(motor, io, safety, adv), commands(io), outputs(io, adv),
      ramp(motor), teachSeq(*this)
{
}

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

const MoveTrace& GateController::getTrace() const
{
    return trace;
}

void GateController::setCompleteCloseHandler(std::function<void()> handler)
{
    onCompleteClose = std::move(handler);
}

void GateController::setEntryPointHandler(std::function<void()> handler)
{
    onEntryPoint = std::move(handler);
}

void GateController::start()
{
    const int64_t now = nowMs();
    lastStartMs = now;     // C#: DateTime.UtcNow at startup
    lastStopMs = now;
    lastUpdateMs = now;

    commandQueue = xQueueCreate(kQueueLength, sizeof(GateCommand));
    xTaskCreate(&GateController::taskEntry, "gate", kTaskStack, this, kTaskPriority, &task);
}

void GateController::taskEntry(void* arg)
{
    static_cast<GateController*>(arg)->run();
}

void GateController::run()
{
    teachSeq.checkResetButton();   // State_CheckLearn()

    // Each pass is one (re)start of the state machine: at boot, and after an ops-limit restart.
    while (true) {
        restartRequested = false;

        ESP_LOGD(TAG, "TeachPhase: %s", toString(teachSeq.getPhase()));

        if (onEntryPoint) {
            onEntryPoint();   // wifi: read the Wi-Fi switch (was PollWifiButton())
        }

        if (skipHomingOnNextEntry) {
            skipHomingOnNextEntry = false;
            ESP_LOGD(TAG, "Max run time restart: stopping motor before resuming (skipping teach/re-homing).");
            stopMotor();

            if (step == MainLoopStep::Opening) {
                setMainLoopStep(MainLoopStep::OpeningNormalStop, "Max run time restart");
            } else if (step == MainLoopStep::Closing) {
                setMainLoopStep(MainLoopStep::ClosingNormalStop, "Max run time restart");
            }
        } else {
            setMainLoopStep(MainLoopStep::Teach, "EntryPoint initialization");
            teachSeq.run();
        }

        normalOperation();   // returns only when restartRequested is set
    }
}
