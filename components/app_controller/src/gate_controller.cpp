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

void GateController::setMainLoopStep(MainLoopStep newStep, const char* reason)
{
    if (step != newStep) {
        ESP_LOGD(TAG, "State: %s | Reason: %s", toString(newStep), reason);
        step = newStep;
    }
}

// Which movement the current safety step interrupted (for the backtrack direction).
Interrupted GateController::interruptedMovement() const
{
    switch (step) {
    case MainLoopStep::SafetyOpenAfterOpening:
    case MainLoopStep::SafetyOpenCloseAfterOpening:
        return Interrupted::Opening;
    case MainLoopStep::SafetyCloseAfterClosing:
    case MainLoopStep::SafetyOpenCloseAfterClosing:
        return Interrupted::Closing;
    default:
        return Interrupted::Unknown;
    }
}

void GateController::stateClosing()
{
    // 1. Safety transition: exit immediately if triggered
    if (!safety.closeOk()) {
        ESP_LOGD(TAG, "Safety triggered: SF_C()");

        if (atPedestrianPosition && checkPedestrianZone()) {
            commands.pedestrianPressed = true;
            atPedestrianPosition = false;
        }
        setMainLoopStep(MainLoopStep::SafetyCloseAfterClosing, "Safety triggered during closing");
        return;
    } else if (!safety.edgeOk()) {
        ESP_LOGD(TAG, "Safety triggered: SF_O_C()");
        setMainLoopStep(MainLoopStep::SafetyOpenCloseAfterClosing, "Safety triggered during closing");
        return;
    }

    // 2. Stop/limit transition: exit immediately if triggered
    if (commands.latchedOsc || commands.openPressed || lscActivated) {
        ESP_LOGD(TAG, "Stop/limit triggered");
        commands.openPressed = false;
        commands.latchedOsc = false;
        if (lscActivated) {
            position = 0;
        }
        setMainLoopStep(MainLoopStep::ClosingNormalStop, "Stop/limit triggered during closing");
        adv.increaseStopCount();
        stopCountSaved = false;
        return;
    }

    // 3. Normal closing operation
    if (completeOpen && !io.isHigh(Input::Lso)) {
        completeOpen = false;
        lsoActivated = false;
        teach.setPosA(teach.getPosO());
        position = teach.getPosO();
    }

    // Pedestrian: always stop at the pedestrian opening
    if (pedestrianMode) {
        const int64_t distanceToPedes = position - pedestrianPulses();
        if (distanceToPedes <= 0) {
            pedestrianMode = false;
            atPedestrianPosition = true;
            setMainLoopStep(MainLoopStep::ClosingNormalStop, "Pedestrian hold: reached pedestrian opening");
            return;
        }
    }

    // 4. Set motor speed and start closing
    startMotor(Direction::Close);
    setMotorSpeed();

    // 5. Pedestrian mode handling
    if (commands.pedestrianPressed) {
        switch (checkPedestrianDirection()) {
        case 1:    // close gate to pedestrian
            pedestrianMode = true;
            commands.pedestrianPressed = false;
            setMainLoopStep(MainLoopStep::Closing, "Pedestrian mode closing");
            break;
        case 0:    // close gate completely
            commands.pedestrianPressed = false;
            setMainLoopStep(MainLoopStep::ClosingNormalStop, "Pedestrian mode closing complete");
            break;
        case -1:   // open gate to pedestrian (pedestrianPressed stays set, as in C#)
            pedestrianMode = true;
            setMainLoopStep(MainLoopStep::ClosingNormalStop, "Pedestrian mode opening");
            break;
        }
    }
}

void GateController::stateClosingNormalStop()
{
    stopMotor();

    // Stop any ramp and reset the torque retry counter
    ramp.clear();
    torqueRetryCounter = 0;

    // Post-teach and pedestrian hold
    if (closeAfterTeach && commands.pedestrianHold && !commands.autoClose) {
        commands.pedestrianPressed = true;
    }
    closeAfterTeach = false;

    completeClose = lscActivatedLatched && !moving;

    if (completeClose) {
        justClosed = true;
        if (onCompleteClose) {
            onCompleteClose();   // wifi: switch off after 5 min (was in this function)
        }
    }

    if (lscActivated && completeClose) {
        // LSC reached: stop and clear the torque alarm, then handle buttons below
        motor.setRun(false);
        motor.setSpeed(0.0f);
        safetyActions.resetTorqueLimitIfTriggered(false, interruptedMovement(), actualDirectionOpening);
    } else if (!lscActivated && completeClose) {
        // Marked complete but LSC not reached: push with extra force
        motor.setSpeed(static_cast<float>(adv.getSpdExtForce()));
        motor.setTorque(adv.getTqOps());
        motor.setDirection(Direction::Close);
        motor.setRun(true);
        ESP_LOGD(TAG, "[EXTRA FORCE] SPD_EXT_FORCE: %.2f", adv.getSpdExtForce());
        safetyActions.resetTorqueLimitIfTriggered(false, interruptedMovement(), actualDirectionOpening);
        return;
    }

    // Wait for the motor to stop
    while (updatePositionAndRpm(true, false) > 0) {
        pollAndLatchInputCommands();
        sleepMs(kPollIntervalMs);
    }

    passageLightOffAfterMotorStop();

    // Save the stop counter if it changed
    if (!stopCountSaved) {
        stopCountSaved = true;
        store.saveStopCount(adv.getStopCount());
    }

    isLsoActivated();

    // Duration test: simulate an open command after the gate is completely closed
    if (adv.getSimOpenEnabled() && completeClose) {
        if (simOpenClosedSinceMs < 0) {
            simOpenClosedSinceMs = nowMs();
        }
        if (secondsSince(simOpenClosedSinceMs) >= adv.getSimOpenDelayS()) {
            simOpenClosedSinceMs = -1;
            pedestrianMode = false;
            atPedestrianPosition = false;
            commands.latchedOpen = false;
            commands.openPressed = false;
            commands.latchedOsc = false;   // open wins, drop any pending OSC
            setMainLoopStep(MainLoopStep::Opening, "Simulated open command (duration test)");
            return;
        }
    } else {
        simOpenClosedSinceMs = -1;
    }

    // Auto-close
    if (commands.autoClose && (checkPedestrianDirection() == 0 || lsoActivated) &&
        secondsSince(lastStopMs) > pub.getAutoCloseDelay()) {
        if (commands.pedestrianHold) {
            return;   // no auto-close while pedestrian is held
        }
        commands.latchedOsc = false;
        setMainLoopStep(MainLoopStep::Closing, "Auto-close triggered");
        return;
    }

    // Buttons (priority: Open, Close, OSC, Pedestrian)
    if (commands.latchedOpen || commands.openPressed) {
        pedestrianMode = false;
        atPedestrianPosition = false;
        commands.latchedOpen = false;
        commands.openPressed = false;

        if (!lsoActivated) {   // only if the gate is not fully open
            commands.latchedOsc = false;   // new movement starts: drop pending OSC
            if (checkPedestrianDirection() == -1 && commands.pedestrianHold) {
                pedestrianMode = true;
            }
            setMainLoopStep(MainLoopStep::Opening, "Open button pressed (latched or direct)");
        }
        return;
    }

    isLscActivated();

    if (commands.latchedClose || commands.closePressed) {
        pedestrianMode = false;
        atPedestrianPosition = false;
        commands.latchedClose = false;
        commands.closePressed = false;

        if (!completeClose && !lscActivated) {   // only if the gate is not fully closed
            commands.latchedOsc = false;
            if (checkPedestrianDirection() == 1 && commands.pedestrianHold) {
                pedestrianMode = true;
            }
            setMainLoopStep(MainLoopStep::Closing, "Close button pressed (latched or direct)");
        }
        return;
    }

    if (commands.latchedOsc) {
        pedestrianMode = false;
        atPedestrianPosition = false;
        commands.latchedOsc = false;

        const int pedDir = checkPedestrianDirection();
        if ((pedDir == -1 || pedDir == 1) && commands.pedestrianHold) {
            pedestrianMode = true;
        }
        setMainLoopStep(MainLoopStep::Opening, "OSC button pressed (latched)");
        return;
    }

    if (commands.latchedPedestrian || commands.pedestrianPressed) {
        commands.latchedPedestrian = false;
        commands.pedestrianPressed = false;
        commands.latchedOsc = false;

        switch (checkPedestrianDirection()) {
        case 1:  pedestrianMode = true;  setMainLoopStep(MainLoopStep::Closing, "Pedestrian mode closing"); break;
        case 0:  pedestrianMode = false; setMainLoopStep(MainLoopStep::Closing, "Pedestrian mode closing complete"); break;
        case -1: pedestrianMode = true;  setMainLoopStep(MainLoopStep::Opening, "Pedestrian mode opening"); break;
        }
        return;
    }

    updatePedestrianMode();
}

void GateController::stateOpening()
{
    // 1. Safety transition: exit immediately if triggered
    if (!safety.openOk()) {
        ESP_LOGD(TAG, "Safety triggered: SF_O()");
        setMainLoopStep(MainLoopStep::SafetyOpenAfterOpening, "Safety triggered during opening");
        return;
    } else if (!safety.edgeOk()) {
        ESP_LOGD(TAG, "Safety triggered: SF_O_C()");
        setMainLoopStep(MainLoopStep::SafetyOpenCloseAfterOpening, "Safety triggered during opening");
        return;
    }

    // 2. Stop/limit transition: exit immediately if triggered
    if (commands.latchedOsc || commands.closePressed || lsoActivated) {
        ESP_LOGD(TAG, "Stop/limit triggered");
        commands.closePressed = false;
        commands.latchedOsc = false;
        pedestrianMode = false;
        setMainLoopStep(MainLoopStep::OpeningNormalStop, "Stop/limit triggered during opening");
        return;
    }

    // 3. Normal opening operation
    if (completeClose && !io.isHigh(Input::Lsc)) {
        completeClose = false;
        lscActivated = false;
        teach.setPosA(0);
        position = 0;
    }

    // Pedestrian: always stop at the pedestrian opening
    if (pedestrianMode) {
        const int64_t distanceToPedes = position - pedestrianPulses();
        if (distanceToPedes >= 0) {
            pedestrianMode = false;
            atPedestrianPosition = true;
            setMainLoopStep(MainLoopStep::OpeningNormalStop, "Pedestrian hold: reached pedestrian opening");
            return;
        }
    }

    // 4. Set motor speed and start opening
    startMotor(Direction::Open);
    setMotorSpeed();

    // 5. Pedestrian mode handling
    if (commands.pedestrianPressed) {
        switch (checkPedestrianDirection()) {
        case 1:    // close gate to pedestrian
            pedestrianMode = true;
            commands.pedestrianPressed = false;
            setMainLoopStep(MainLoopStep::OpeningNormalStop, "Pedestrian mode closing");
            break;
        case 0:    // close gate completely
            commands.pedestrianPressed = false;
            setMainLoopStep(MainLoopStep::OpeningNormalStop, "Pedestrian mode closing complete");
            break;
        case -1:   // open gate to pedestrian
            pedestrianMode = true;
            commands.pedestrianPressed = false;
            setMainLoopStep(MainLoopStep::Opening, "Pedestrian mode opening");
            break;
        }
    }
}

void GateController::stateOpeningNormalStop()
{
    stopMotor();

    ramp.clear();
    torqueRetryCounter = 0;

    completeOpen = lsoActivatedLatched && !moving;

    if (lsoActivated && completeOpen) {
        // LSO reached: stop and clear the torque alarm, then handle buttons below
        motor.setRun(false);
        motor.setSpeed(0.0f);
        safetyActions.resetTorqueLimitIfTriggered(false, interruptedMovement(), actualDirectionOpening);
    } else if (!lsoActivated && completeOpen) {
        // Marked complete but LSO not reached: push with extra force
        motor.setSpeed(static_cast<float>(adv.getSpdExtForce()));
        motor.setTorque(adv.getTqOps());
        motor.setDirection(Direction::Open);
        motor.setRun(true);
        ESP_LOGD(TAG, "[EXTRA FORCE] SPD_EXT_FORCE: %.2f", adv.getSpdExtForce());
        safetyActions.resetTorqueLimitIfTriggered(false, interruptedMovement(), actualDirectionOpening);
        return;
    }

    // Wait for the motor to stop
    while (updatePositionAndRpm(true, false) > 0) {
        pollAndLatchInputCommands();
        sleepMs(kPollIntervalMs);
    }

    passageLightOffAfterMotorStop();

    if (!stopCountSaved) {
        stopCountSaved = true;
        store.saveStopCount(adv.getStopCount());
    }

    isLsoActivated();

    // Auto-close
    if (commands.autoClose && (checkPedestrianDirection() == 0 || lsoActivated) &&
        secondsSince(lastStopMs) > pub.getAutoCloseDelay() &&
        !commands.openPressed && !commands.latchedOpen) {
        if (commands.pedestrianHold) {
            return;
        }
        commands.latchedOsc = false;
        setMainLoopStep(MainLoopStep::Closing, "Auto-close triggered");
        return;
    }

    if (commands.latchedOpen || commands.openPressed) {
        pedestrianMode = false;
        atPedestrianPosition = false;
        commands.latchedOpen = false;
        commands.openPressed = false;

        if (!lsoActivated) {
            commands.latchedOsc = false;
            if (checkPedestrianDirection() == -1 && commands.pedestrianHold) {
                pedestrianMode = true;
            }
            setMainLoopStep(MainLoopStep::Opening, "Open button pressed (latched or direct)");
        }
        return;
    }

    if (commands.latchedClose || commands.closePressed) {
        pedestrianMode = false;
        atPedestrianPosition = false;
        commands.latchedClose = false;
        commands.closePressed = false;
        commands.latchedOsc = false;   // close wins, drop any pending OSC

        if (checkPedestrianDirection() == 1 && commands.pedestrianHold) {
            pedestrianMode = true;
        }
        setMainLoopStep(MainLoopStep::Closing, "Close button pressed (latched or direct)");
        return;
    }

    if (commands.latchedOsc) {
        pedestrianMode = false;
        atPedestrianPosition = false;
        commands.latchedOsc = false;

        const int pedDir = checkPedestrianDirection();
        if ((pedDir == -1 || pedDir == 1) && commands.pedestrianHold) {
            pedestrianMode = true;
        }
        setMainLoopStep(MainLoopStep::Closing, "OSC button pressed (latched)");
        return;
    }

    if (commands.latchedPedestrian || commands.pedestrianPressed) {
        commands.latchedPedestrian = false;
        commands.pedestrianPressed = false;
        commands.latchedOsc = false;

        switch (checkPedestrianDirection()) {
        case 1:  pedestrianMode = true;  setMainLoopStep(MainLoopStep::Closing, "Pedestrian mode closing"); break;
        case 0:  pedestrianMode = false; setMainLoopStep(MainLoopStep::Closing, "Pedestrian mode closing complete"); break;
        case -1: pedestrianMode = true;  setMainLoopStep(MainLoopStep::Opening, "Pedestrian mode opening"); break;
        }
        return;
    }

    updatePedestrianMode();
}
