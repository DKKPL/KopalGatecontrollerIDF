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

// Any task: queue a command for the gate task.
bool GateController::post(GateCommand cmd)
{
    return commandQueue != nullptr && xQueueSend(commandQueue, &cmd, 0) == pdTRUE;
}

// Any task: copy of the latest status.
GateStatus GateController::getStatus() const
{
    statusMu.lock();
    const GateStatus copy = status;
    statusMu.unlock();
    return copy;
}

// Gate task: refresh the snapshot.
void GateController::publishStatus()
{
    GateStatus s;
    s.step = step;
    s.teachPhase = teachSeq.getPhase();
    s.position = position;
    s.rpm = rpm;
    s.speedDuty = motor.getSpeed();
    s.torqueDuty = motor.getTorque();
    s.moving = moving;
    s.locked = outputs.isLocked();
    s.flash = outputs.isFlashOn();
    s.passageLight = outputs.isPassageLightOn();
    s.autoClose = commands.autoClose;
    s.pedestrianMode = pedestrianMode;
    s.completeOpen = completeOpen;
    s.completeClose = completeClose;
    s.photocellTriggered = safety.isPhotocellTriggered();
    s.safetyEdgeTriggered = safety.isSafetyEdgeTriggered();
    s.torqueLimitTriggered = safety.isTorqueLimitTriggered();
    s.actualDirectionOpening = actualDirectionOpening;
    s.lsoActivated = lsoActivated;
    s.lscActivated = lscActivated;

    statusMu.lock();
    status = s;
    statusMu.unlock();
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

void GateController::normalOperation()
{
    int64_t lastPrintMs = 0;
    bool lastDirectionHigh = io.hasFeedbackDirection() ? io.isHigh(Input::FeedbackDirection) : false;

    // Main operation loop: poll inputs, update state, dispatch the state handler.
    while (!restartRequested) {
        // Direction pin (only wired when the quadrature encoder is not used).
        if (io.hasFeedbackDirection()) {
            const bool directionHigh = io.isHigh(Input::FeedbackDirection);
            if (directionHigh != lastDirectionHigh) {
                actualDirectionOpening = !directionHigh;   // C#: (value == PinValue.Low)
                lastDirectionHigh = directionHigh;
            }
        }

        if (moving) {
            updatePositionAndRpm();

            if (trace.due()) {
                const char traceDir = step == MainLoopStep::Closing ? 'C'
                                    : step == MainLoopStep::Opening ? 'O' : '?';
                const char traceRamp = ramp.rampingDown() ? 'D' : (ramp.rampingUp() ? 'U' : '-');
                trace.record(position, motor.getSpeed(), rpm, traceDir, traceRamp,
                              io.isHigh(Input::Lso), io.isHigh(Input::Lsc));
            }
        }

        if (nowMs() - lastPrintMs >= 2000) {
            ESP_LOGD(TAG, "Status: counterTotal=%lld, torque=%.2f, speed (rpm)=%.2f, POS_O=%lld",
                     static_cast<long long>(position), motor.getTorque(), rpm,
                     static_cast<long long>(teach.getPosO()));
            lastPrintMs = nowMs();
        }

        // Centralized polling
        commands.pollOpen();
        commands.pollClose();
        commands.pollOsc();
        commands.pollPedestrian();
        commands.pollAutoClose();

        drainCommands();   // was PollRemoteCommands(); ProcessModbusCli/DrainMbLog -> Modbus task

        isLsoActivated();
        isLscActivated();

        safety.pollPhotocell();
        safety.pollSafetyEdge();
        safetyActions.handleSafetyEdgeIfNeeded(adv.getBtAftSe(), interruptedMovement(), actualDirectionOpening);
        safety.pollTorqueLimit();

        engageLockAfterMotorStop();
        passageLightOffAfterMotorStop();

        // Reset the auto-close timer while SF_C is triggered.
        if (commands.autoClose && !safety.closeOk()) {
            lastStopMs = nowMs();
            ESP_LOGD(TAG, "AutoClose timer reset due to SF_C trigger.");
        }

        switch (step) {
        case MainLoopStep::Closing:                     stateClosing(); break;
        case MainLoopStep::ClosingNormalStop:           stateClosingNormalStop(); break;
        case MainLoopStep::Opening:                     stateOpening(); break;
        case MainLoopStep::OpeningNormalStop:           stateOpeningNormalStop(); break;
        case MainLoopStep::SafetyCloseAfterClosing:     stateSafetyCloseAfterClosing(); break;
        case MainLoopStep::SafetyOpenCloseAfterClosing: stateSafetyOpenCloseAfterClosing(); break;
        case MainLoopStep::SafetyOpenAfterOpening:      stateSafetyOpenAfterOpening(); break;
        case MainLoopStep::SafetyOpenCloseAfterOpening: stateSafetyOpenCloseAfterOpening(); break;
        default: break;
        }

        ramp.tick();       // new: replaces RampUpThread / RampDownThread
        publishStatus();    // new: snapshot for Modbus / web
        sleepMs(kPollIntervalMs);
    }
}
