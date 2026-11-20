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
    // New: blocking waits in these parts refresh the status first, so Modbus sees the outputs change.
    outputs.setWaitHandler([this] { publishStatus(); });
    safetyActions.setWaitHandler([this] { publishStatus(); });
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

        ESP_LOGI(TAG, "TeachPhase: %s", toString(teachSeq.getPhase()));

        if (onEntryPoint) {
            onEntryPoint();   // wifi: read the Wi-Fi switch (was PollWifiButton())
        }

        if (skipHomingOnNextEntry) {
            skipHomingOnNextEntry = false;
            ESP_LOGI(TAG, "Max run time restart: stopping motor before resuming (skipping teach/re-homing).");
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
        ESP_LOGI(TAG, "State: %s | Reason: %s", toString(newStep), reason);
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
        ESP_LOGI(TAG, "Safety triggered: SF_C()");

        if (atPedestrianPosition && checkPedestrianZone()) {
            commands.pedestrianPressed = true;
            atPedestrianPosition = false;
        }
        setMainLoopStep(MainLoopStep::SafetyCloseAfterClosing, "Safety triggered during closing");
        return;
    } else if (!safety.edgeOk()) {
        ESP_LOGI(TAG, "Safety triggered: SF_O_C()");
        setMainLoopStep(MainLoopStep::SafetyOpenCloseAfterClosing, "Safety triggered during closing");
        return;
    }

    // 2. Stop/limit transition: exit immediately if triggered
    if (commands.latchedOsc || commands.openPressed || lscActivated) {
        ESP_LOGI(TAG, "Stop/limit triggered");
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
        ESP_LOGI(TAG, "Safety triggered: SF_O()");
        setMainLoopStep(MainLoopStep::SafetyOpenAfterOpening, "Safety triggered during opening");
        return;
    } else if (!safety.edgeOk()) {
        ESP_LOGI(TAG, "Safety triggered: SF_O_C()");
        setMainLoopStep(MainLoopStep::SafetyOpenCloseAfterOpening, "Safety triggered during opening");
        return;
    }

    // 2. Stop/limit transition: exit immediately if triggered
    if (commands.latchedOsc || commands.closePressed || lsoActivated) {
        ESP_LOGI(TAG, "Stop/limit triggered");
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

void GateController::stateSafetyCloseAfterClosing()
{
    stopMotor();
    ramp.clear();

    if (safety.pollTorqueLimit()) {
        ++torqueRetryCounter;
        ESP_LOGI(TAG, "Safety retry count: %d/%d", torqueRetryCounter, adv.getTorqueRetryMaxCount());

        // Always perform the reset/backtrack, even on the last retry
        safetyActions.resetTorqueLimitIfTriggered(adv.getBtAftTqLmt(), interruptedMovement(), actualDirectionOpening);

        if (torqueRetryCounter > adv.getTorqueRetryMaxCount()) {
            ESP_LOGI(TAG, "Safety retry max reached. Stopping further retries.");
            setMainLoopStep(MainLoopStep::ClosingNormalStop, "Retry max reached in safety close after closing");
        }
        return;
    }

    if (!adv.getWaitScDc()) {
        if (atPedestrianPosition && commands.pedestrianHold) {
            pedestrianMode = true;   // keep pedestrian mode for the resumed opening
        }
        setMainLoopStep(MainLoopStep::Opening, "Safety close after closing interrupt");
    } else if (adv.getSafetyResume()) {
        waitForAllSafetiesOkAndLatchInputs();
        setMainLoopStep(MainLoopStep::Closing, "Safety close after closing resume");
    } else {
        waitForAllSafetiesOkAndLatchInputs();
        setMainLoopStep(MainLoopStep::ClosingNormalStop, "Safety close after closing normal stop");
    }
}

void GateController::stateSafetyOpenAfterOpening()
{
    stopMotor();
    ramp.clear();

    if (safety.pollTorqueLimit()) {
        ++torqueRetryCounter;
        ESP_LOGI(TAG, "Safety retry count: %d/%d", torqueRetryCounter, adv.getTorqueRetryMaxCount());

        safetyActions.resetTorqueLimitIfTriggered(adv.getBtAftTqLmt(), interruptedMovement(), actualDirectionOpening);

        if (torqueRetryCounter > adv.getTorqueRetryMaxCount()) {
            ESP_LOGI(TAG, "Safety retry max reached. Stopping further retries.");
            setMainLoopStep(MainLoopStep::OpeningNormalStop, "Retry max reached in safety open after opening");
        }
        return;
    }

    if (!adv.getWaitSoDo()) {
        setMainLoopStep(MainLoopStep::Closing, "Safety open after opening interrupt");
    } else if (adv.getSafetyResume()) {
        waitForAllSafetiesOkAndLatchInputs();
        setMainLoopStep(MainLoopStep::Opening, "Safety open after opening resume");
    } else {
        waitForAllSafetiesOkAndLatchInputs();
        setMainLoopStep(MainLoopStep::OpeningNormalStop, "Safety open after opening normal stop");
    }
}

void GateController::stateSafetyOpenCloseAfterClosing()
{
    stopMotor(true);
    ramp.clear();

    waitForAllSafetiesOkAndLatchInputs();

    // Latched commands first
    if (commands.latchedOpen || commands.openPressed) {
        atPedestrianPosition = false;
        commands.latchedOpen = false;
        commands.openPressed = false;
        setMainLoopStep(MainLoopStep::Opening, "Open button pressed after safety");
        return;
    }
    if (commands.latchedClose || commands.closePressed) {
        atPedestrianPosition = false;
        commands.latchedClose = false;
        commands.closePressed = false;
        setMainLoopStep(MainLoopStep::Closing, "Close button pressed after safety");
        return;
    }
    if (commands.latchedOsc) {
        atPedestrianPosition = false;
        commands.latchedOsc = false;
        setMainLoopStep(MainLoopStep::Opening, "OSC button pressed after safety");
        return;
    }
    if (commands.latchedPedestrian || commands.pedestrianPressed) {
        atPedestrianPosition = false;
        commands.latchedPedestrian = false;
        commands.pedestrianPressed = false;
        switch (checkPedestrianDirection()) {
        case 1:  pedestrianMode = true;  setMainLoopStep(MainLoopStep::Closing, "Pedestrian mode closing after safety"); break;
        case 0:  pedestrianMode = false; setMainLoopStep(MainLoopStep::Closing, "Pedestrian mode closing complete after safety"); break;
        case -1: pedestrianMode = true;  setMainLoopStep(MainLoopStep::Opening, "Pedestrian mode opening after safety"); break;
        }
        return;
    }

    // No command latched: resume as configured
    if (!adv.getWaitSocDc()) {
        setMainLoopStep(MainLoopStep::Opening, "Safety open close after closing interrupt");
    } else if (adv.getSafetyResume()) {
        setMainLoopStep(MainLoopStep::Closing, "Safety open close after closing resume");
    } else {
        setMainLoopStep(MainLoopStep::ClosingNormalStop, "Safety open close after closing normal stop");
    }
}

void GateController::stateSafetyOpenCloseAfterOpening()
{
    stopMotor(true);
    ramp.clear();

    waitForAllSafetiesOkAndLatchInputs();

    // Latched commands first
    if (commands.latchedOpen || commands.openPressed) {
        atPedestrianPosition = false;
        commands.latchedOpen = false;
        commands.openPressed = false;
        setMainLoopStep(MainLoopStep::Opening, "Open button pressed after safety");
        return;
    }
    if (commands.latchedClose || commands.closePressed) {
        atPedestrianPosition = false;
        commands.latchedClose = false;
        commands.closePressed = false;
        setMainLoopStep(MainLoopStep::Closing, "Close button pressed after safety");
        return;
    }
    if (commands.latchedOsc) {
        atPedestrianPosition = false;
        commands.latchedOsc = false;
        setMainLoopStep(MainLoopStep::Opening, "OSC button pressed after safety");
        return;
    }
    if (commands.latchedPedestrian || commands.pedestrianPressed) {
        atPedestrianPosition = false;
        commands.latchedPedestrian = false;
        commands.pedestrianPressed = false;
        switch (checkPedestrianDirection()) {
        case 1:  pedestrianMode = true;  setMainLoopStep(MainLoopStep::Closing, "Pedestrian mode closing after safety"); break;
        case 0:  pedestrianMode = false; setMainLoopStep(MainLoopStep::Closing, "Pedestrian mode closing complete after safety"); break;
        case -1: pedestrianMode = true;  setMainLoopStep(MainLoopStep::Opening, "Pedestrian mode opening after safety"); break;
        }
        return;
    }

    // No command latched: resume as configured
    if (!adv.getWaitSocDo()) {
        setMainLoopStep(MainLoopStep::Closing, "Safety open close after opening interrupt");
    } else if (adv.getSafetyResume()) {
        setMainLoopStep(MainLoopStep::Opening, "Safety open close after opening resume");
    } else {
        setMainLoopStep(MainLoopStep::OpeningNormalStop, "Safety open close after opening normal stop");
    }
}

void GateController::startMotor(Direction direction)
{
    motor.enableForMovement();   // EnsureMotorDriverEnabledForMovement()

    lscActivatedLatched = false;
    lsoActivatedLatched = false;

    // 1. Do nothing if already moving
    if (moving) {
        return;
    }

    ramp.clear();   // every movement starts from a clean ramp
    trace.reset();

    loggedLowSpeedClosing = false;
    loggedLowSpeedOpening = false;

    // 3. Passage light on
    if (!outputs.isPassageLightOn()) {
        outputs.passageLightOn();
    }

    // 4. Unlock if needed
    if (outputs.isLocked()) {
        ESP_LOGI(TAG, "[StartMotor] Lock is engaged, unlocking...");

        const bool preWarning = secondsSince(lastStopMs) > adv.getGateIdleLockTimeoutSeconds() || justClosed;
        outputs.disengageLock(preWarning ? pub.getBeaconPreWarningTime() * 1000 : 0);
        if (preWarning) {
            justClosed = false;
        }

        // Unlock movement: briefly drive in the close direction to free the solenoid
        if (direction == Direction::Open && adv.getSolenoidMagnet()) {
            ESP_LOGI(TAG, "[StartMotor] Performing unlock movement.");
            motor.setSpeed(static_cast<float>(adv.getSpdUnlck()));
            motor.setTorque(adv.getTqOps());
            motor.setDirection(Direction::Close);
            motor.setRun(true);
            publishStatus();   // new: keep the Modbus status current during the wait
            sleepMs(adv.getUnlckMoveTime());
        }
    }

    // 5. Requested direction
    motor.setDirection(direction);

    // 6. Start the motor
    setMotorSpeed();
    motor.setRun(true);
    ESP_LOGI(TAG, "Motor started.");

    // 7. State and timestamps
    lastStartMs = nowMs();
    moving = true;
}

void GateController::stopMotor(bool forceStop)
{
    // Always drop the ramp, even when not moving.
    ramp.clear();

    if (!moving) {
        return;
    }

    motor.setRun(false);
    motor.setSpeed(0.0f);
    motor.setTorque(adv.getTqSfty());
    lastStopMs = nowMs();
    moving = false;

    if (forceStop) {
        motor.toggleDirection();
        ESP_LOGI(TAG, "Force stop executed");
    } else {
        ESP_LOGI(TAG, "Normal stop executed");
    }
}

// Movement took longer than the max run time: restart the state machine (skip re-homing).
void GateController::checkMaxRunTimeExceeded()
{
    if (moving && secondsSince(lastStartMs) > adv.getMaxRunTime()) {
        ESP_LOGI(TAG, "[DEBUG] Max run time reached, requesting restart (no recursive EntryPoint call)");
        restartRequested = true;
        skipHomingOnNextEntry = true;
    }
}

void GateController::setMotorSpeed()
{
    switch (step) {
    case MainLoopStep::Teach:
        // Teach / homing: slow teach speed and the low safety torque.
        motor.setSpeed(static_cast<float>(adv.getSpdTch()));
        motor.setTorque(adv.getTqSfty());
        break;

    case MainLoopStep::Opening:
        // Near the end (APP_O pulses before fully open, or before the pedestrian position): slow down.
        // Otherwise: ramp up to the fast opening speed.
        if (position >= teach.getPosO() - adv.getAppOPulses() ||
            (pedestrianMode && position >= pedestrianPulses() - adv.getAppOPulses())) {
            if (!loggedLowSpeedOpening) {
                loggedLowSpeedOpening = true;
                ESP_LOGI(TAG, "[SPEED] Opening low speed activated: counterTotal=%lld, APP_O_Pulses=%d, POS_O=%lld",
                         static_cast<long long>(position), adv.getAppOPulses(),
                         static_cast<long long>(teach.getPosO()));
            }
            ramp.tryStartDown(adv.getSpdLoO(), adv.getDecO());
        } else {
            ramp.tryStartUp(adv.getSpdHiO(), adv.getAccO());
        }
        motor.setTorque(adv.getTqOps());   // opening always uses normal torque
        break;

    case MainLoopStep::Closing:
        // Near the end (APP_C pulses before fully closed, or before the pedestrian position): slow down.
        // Otherwise: ramp up to the fast closing speed.
        if (position <= adv.getAppCPulses() ||
            (pedestrianMode && position <= pedestrianPulses() + adv.getAppCPulses())) {
            if (!loggedLowSpeedClosing) {
                loggedLowSpeedClosing = true;
                ESP_LOGI(TAG, "[SPEED] Closing low speed activated: counterTotal=%lld, APP_C_Pulses=%d",
                         static_cast<long long>(position), adv.getAppCPulses());
            }
            ramp.tryStartDown(adv.getSpdLoC(), adv.getDecC());
        } else {
            ramp.tryStartUp(adv.getSpdHiC(), adv.getAccC());
        }

        // Torque safety zone near closed (between TQ_STFY_ZONE_STOP and _STRT): low torque, so the gate pushes softly.
        if (position <= adv.getTqSftyZoneStrtPulses() && position >= adv.getTqSftyZoneStopPulses()) {
            if (lastMotorSpeedStep != MainLoopStep::Closing) {
                // The close starts inside the zone: normal torque for 1.5 s to get the gate moving.
                motor.setTorque(adv.getTqOps());
                safetyZoneStartMs = nowMs();   // sftyZoneStopWatch.Restart()
            } else if (safetyZoneStartMs >= 0 && nowMs() - safetyZoneStartMs <= 1500) {
                // Still within those 1.5 s: keep normal torque.
                motor.setTorque(adv.getTqOps());
            } else {
                // Entered the zone while moving, or the 1.5 s are over: low safety torque.
                motor.setTorque(adv.getTqSfty());
                safetyZoneStartMs = -1;        // Stop() + Reset()
            }
        } else {
            motor.setTorque(adv.getTqOps());   // outside the zone: normal torque
        }
        break;

    default:
        // Any other step: fixed slow speed and the low safety torque.
        motor.setSpeed(0.41176f);
        motor.setTorque(adv.getTqSfty());
        break;
    }

    lastMotorSpeedStep = step;   // lets the next call see whether a close just started
}

int GateController::updatePositionAndRpm(bool updateRpm, bool updatePosition)
{
    // Created on first use, like the C# null check (quadrature: Hall A + B, else Hall A only).
    if (!counter.isStarted()) {
        counter.begin(adv.getUseQuadratureEncoder());
    }

    // pulseCounter.Reset(): read the count and set it back to 0.
    const int32_t actualCount = counter.takeCount();

    const int64_t now = nowMs();
    if (updateRpm) {
        const int64_t dtMs = now - lastUpdateMs;
        if (dtMs > 0) {
            const float rotationsPerMs = static_cast<float>(actualCount) / adv.getEncPpr() / static_cast<float>(dtMs);
            rpm = std::fabs(rotationsPerMs * 60.0f * 1000.0f);
        }
    }
    lastUpdateMs = now;

    if (updatePosition) {
        if (adv.getUseQuadratureEncoder()) {
            // Signed count: positive = opening, negative = closing.
            const int64_t delta = adv.getInvertEncoderCount() ? -actualCount : actualCount;
            position += delta;

            // Keep the direction in sync (no feedback pin in quadrature mode).
            if (delta > 0) {
                actualDirectionOpening = true;
            } else if (delta < 0) {
                actualDirectionOpening = false;
            }
        } else {
            // Single pulse: direction comes from the feedback pin.
            const bool effectiveDirection = actualDirectionOpening ^ adv.getInvertEncoderCount();
            if (!effectiveDirection) {
                position += actualCount;
            } else {
                position -= actualCount;
            }
        }
    }

    if (pedestrianMode) {
        const int64_t distanceToPedes = position - pedestrianPulses();
        if (step == MainLoopStep::Closing && distanceToPedes <= 0) {
            pedestrianMode = false;
            atPedestrianPosition = true;
            setMainLoopStep(MainLoopStep::ClosingNormalStop, "Pedestrian mode closing reached");
        } else if (step == MainLoopStep::Opening && distanceToPedes >= 0) {
            pedestrianMode = false;
            atPedestrianPosition = true;
            setMainLoopStep(MainLoopStep::OpeningNormalStop, "Pedestrian mode opening reached");
        }
    }

    checkMaxRunTimeExceeded();
    return static_cast<int>(rpm);
}

bool GateController::isLsoActivated()
{
    const bool activated = io.isHigh(Input::Lso);
    if (activated) {
        lsoActivatedLatched = true;
        if (teach.getPosO() > 0) {
            position = teach.getPosO();
        }
    }
    return lsoActivated = activated;
}

bool GateController::isLscActivated()
{
    const bool activated = io.isHigh(Input::Lsc);
    if (activated) {
        lscActivatedLatched = true;
        position = 0;
    }
    return lscActivated = activated;
}

// _PublicSettings.PedestrianModeDistancePulse
int64_t GateController::pedestrianPulses() const
{
    return pub.getPedestrianModeDistancePulse(adv.getPulsesPerMeter());
}

// 1 = close to the pedestrian position, -1 = open to it, 0 = already there (within the margin).
int GateController::checkPedestrianDirection() const
{
    const int64_t distanceToPedes = position - pedestrianPulses();
    const double margin = adv.getPedesPosMarginPulses();

    if (distanceToPedes > 0 && distanceToPedes - margin > 0) {
        return 1;
    }
    if (distanceToPedes < 0 && distanceToPedes + margin < 0) {
        return -1;
    }
    return 0;
}

// Between closed and the pedestrian position (plus margin).
bool GateController::checkPedestrianZone() const
{
    const double upperBound = pedestrianPulses() + adv.getPedesPosMarginPulses();
    return position >= 0 && position <= upperBound;
}

bool GateController::isInPedestrianArea() const
{
    const int64_t distance = position - pedestrianPulses();
    const double margin = adv.getPedesPosMarginPulses();
    return distance >= -margin && distance <= margin;
}

void GateController::updatePedestrianMode()
{
    if (isInPedestrianArea()) {
        pedestrianMode = false;
    } else if (commands.pedestrianHold) {
        pedestrianMode = true;
    }
}

void GateController::pollSafetyInputs()
{
    safety.pollPhotocell();
    safety.pollSafetyEdge();
    safetyActions.resetTorqueLimitIfTriggered(true, interruptedMovement(), actualDirectionOpening);   // no-op when not triggered
    safety.pollTorqueLimit();
}

// False when a safety tripped and was waited out (the motor is restarted in `direction`).
bool GateController::ensureSafetyAndResume(Direction direction)
{
    // Status check only: no torque recovery while the motor may still be moving.
    safety.pollPhotocell();
    safety.pollSafetyEdge();
    safety.pollTorqueLimit();

    if (safety.allOk()) {
        return true;
    }

    stopMotor();
    while (!safety.allOk()) {
        publishStatus();   // new: keep the Modbus status current during the wait
        sleepMs(kPollIntervalMs);
        pollSafetyInputs();
    }
    ESP_LOGI(TAG, "Safety cleared, starting motor.");
    startMotor(direction);
    return false;
}

void GateController::waitForAllSafetiesOkAndLatchInputs()
{
    commands.clearLatches();

    // Initial poll
    commands.pollOsc();
    commands.pollOpen();
    commands.pollClose();
    commands.pollPedestrian();

    while (!safety.allOk()) {
        sleepMs(kPollIntervalMs);
        pollAndLatchInputCommands();   // keep latching buttons and remote commands
        pollSafetyInputs();
    }
}

void GateController::pollAndLatchInputCommands()
{
    commands.pollOsc();
    if (commands.pollOpen()) {
        commands.latchedOpen = true;
    }
    if (commands.pollClose()) {
        commands.latchedClose = true;
    }
    commands.pollPedestrian();
    if (commands.pedestrianPressed) {
        commands.latchedPedestrian = true;
    }

    drainCommands();   // was PollRemoteCommands()
    publishStatus();
}

// Lock after the idle timeout, or straight away when fully closed.
void GateController::engageLockAfterMotorStop()
{
    if ((secondsSince(lastStopMs) > adv.getGateIdleLockTimeoutSeconds() && !moving) ||
        (step == MainLoopStep::ClosingNormalStop && completeClose)) {
        outputs.engageLock();
    }
}

void GateController::passageLightOffAfterMotorStop()
{
    if (outputs.isPassageLightOn() && !moving &&
        (secondsSince(lastStopMs) > pub.getPassageLightDelay() || completeClose)) {
        outputs.passageLightOff();
    }
}
