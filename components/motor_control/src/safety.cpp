#include "motor_control/safety.h"

#include "config/advanced_settings.h"
#include "esp_log.h"
#include "hal_io/io.h"
#include "hal_motor/motor.h"
#include "motor_control/time_util.h"

static const char* TAG = "safety";

SafetyMonitor::SafetyMonitor(const Io& io, const AdvancedSettings& adv) : io(io), adv(adv)
{
}

SafetyActions::SafetyActions(Motor& motor, const Io& io, SafetyMonitor& monitor, const AdvancedSettings& adv)
    : motor(motor), io(io), monitor(monitor), adv(adv)
{
}

// True when closing is allowed: photocell and torque limit not triggered.
bool SafetyMonitor::closeOk() const
{
    return !photocellTriggered && !torqueLimitTriggered;
}

// True when opening is allowed: torque limit not triggered.
bool SafetyMonitor::openOk() const
{
    return !torqueLimitTriggered;
}

// True when the safety edge is not triggered (blocks both directions).
bool SafetyMonitor::edgeOk() const
{
    return !safetyEdgeTriggered;
}

bool SafetyMonitor::allOk() const
{
    return closeOk() && openOk() && edgeOk();
}

bool SafetyMonitor::pollPhotocell()
{
    return photocellTriggered = io.isHigh(Input::Photocell);
}

bool SafetyMonitor::pollTorqueLimit()
{
    return torqueLimitTriggered = io.isHigh(Input::TorqueLimit);
}

bool SafetyMonitor::pollSafetyEdge()
{
    // The level that represents the normal/closed (safe) state.
    constexpr bool kEdgeOkHigh = true;

    const bool level = io.isHigh(Input::SafetyEdge);

    // Not safe (wire open / fault): report triggered immediately.
    if (level != kEdgeOkHigh) {
        stopEdgeClearTimer();
        safetyEdgeTriggered = true;
        return safetyEdgeTriggered;
    }

    if (safetyEdgeTriggered) {
        // Was triggered: the edge must stay OK for SafetyEdgeWaitTime before we clear it.
        startEdgeClearTimer();
        if (nowMs() - edgeOkSinceMs >= static_cast<int64_t>(adv.safetyEdgeWaitTime()) * 1000) {
            safetyEdgeTriggered = false;
            stopEdgeClearTimer();
        }
    } else {
        // Already safe and stable.
        stopEdgeClearTimer();
        safetyEdgeTriggered = false;
    }
    return safetyEdgeTriggered;
}

void SafetyMonitor::startEdgeClearTimer()
{
    if (edgeOkSinceMs < 0) {
        edgeOkSinceMs = nowMs();
    }
}

// OnSafetyEdgeTimerExpired: no equivalent.
// pollSafetyEdge() checks nowMs() - edgeOkSinceMs instead of counting 1 s ticks.

void SafetyMonitor::stopEdgeClearTimer()
{
    edgeOkSinceMs = -1;
}

// Backtrack means "move opposite to the interrupted movement".
Direction SafetyActions::determineBacktrackDirection(Interrupted interrupted, bool movingOpen) const
{
    switch (interrupted) {
    case Interrupted::Opening:    // SafetyOpenAfterOpening / SafetyOpenCloseAfterOpening
        return Direction::Close;
    case Interrupted::Closing:    // SafetyCloseAfterClosing / SafetyOpenCloseAfterClosing
        return Direction::Open;
    case Interrupted::Unknown:
        break;
    }

    // Prefer the feedback pin (authoritative) if it is in use.
    if (io.hasFeedbackDirection()) {
        const bool feedbackOpen = io.isHigh(Input::FeedbackDirection);   // High = OpenDirection
        return feedbackOpen ? Direction::Close : Direction::Open;
    }

    // Fallback: reverse the last known travel direction.
    return movingOpen ? Direction::Close : Direction::Open;
}

Direction SafetyActions::applyBacktrackDirection(Interrupted interrupted, bool movingOpen, int postWriteDelayMs)
{
    const Direction backtrack = determineBacktrackDirection(interrupted, movingOpen);

    // The caller handles motor enable / torque.
    motor.setDirection(backtrack);
    ESP_LOGD(TAG, "Applied backtrack direction: %s", backtrack == Direction::Open ? "Open" : "Close");

    if (postWriteDelayMs > 0) {
        sleepMs(postWriteDelayMs);
    }
    return backtrack;
}

void SafetyActions::resetTorqueLimitIfTriggered(bool doBacktrack, Interrupted interrupted, bool movingOpen)
{
    if (!monitor.pollTorqueLimit()) {
        return;
    }

    constexpr int kOverallMaxAttempts = 5;
    int overallAttempts = 0;

    while (monitor.pollTorqueLimit() && overallAttempts < kOverallMaxAttempts) {
        ++overallAttempts;
        ESP_LOGD(TAG, "Torque limit triggered. Reset attempt %d/%d.", overallAttempts, kOverallMaxAttempts);

        constexpr int kMaxTries = 20;
        int tries = 0;

        while (monitor.pollTorqueLimit() && tries < kMaxTries) {
            ESP_LOGD(TAG, "Still triggered, toggling motor disable/direction (try %d/%d)...", tries + 1, kMaxTries);
            motor.setDriverDisabled(true);

            if (adv.useDualDirectionInputs()) {
                // No independent direction pin in dual mode: pulse the run state briefly.
                // The driver is disabled, so this cannot cause real movement.
                motor.setDirection(Direction::Open);
                motor.setRun(false);
                sleepMs(200);
                motor.setDirection(Direction::Close);
                motor.setRun(true);
                sleepMs(200);
                motor.setDirection(Direction::Close);
                motor.setRun(false);
            } else {
                motor.setDirection(Direction::Open);
                sleepMs(200);
                motor.setDirection(Direction::Close);
                sleepMs(200);
            }

            motor.setDriverDisabled(false);
            ++tries;
        }
        if (tries == kMaxTries) {
            ESP_LOGD(TAG, "Torque limit did not reset after max tries. Giving up.");
        }

        // Make sure the motor is stopped before changing direction.
        motor.setRun(false);
        motor.setSpeed(1.0f);
        motor.setTorque(adv.tqOps());

        // Apply backtrack direction (writes direction pin and waits).
        applyBacktrackDirection(interrupted, movingOpen);

        if (io.isHigh(Input::TorqueLimit)) {
            ESP_LOGD(TAG, "Torque limit still triggered after reset. Not backtracking.");
            continue;
        }
        if (doBacktrack) {
            ESP_LOGD(TAG, "Backtrack...");
            motor.setRun(true);
            sleepMs(adv.btTime());
            motor.setRun(false);
        }
    }

    if (monitor.pollTorqueLimit()) {
        ESP_LOGD(TAG, "Torque limit persists after attempts. Entering fault/abort state.");
    }

    sleepMs(kPollIntervalMs);
}

// Call after monitor.pollSafetyEdge() in the same poll cycle.
void SafetyActions::handleSafetyEdgeIfNeeded(bool doBacktrack, Interrupted interrupted, bool movingOpen)
{
    if (!monitor.isSafetyEdgeTriggered()) {
        if (safetyEdgeHandled) {
            ESP_LOGD(TAG, "Safety edge cleared, handler reset.");
        }
        safetyEdgeHandled = false;
        return;
    }
    if (safetyEdgeHandled) {
        return;
    }

    safetyEdgeHandled = true;
    ESP_LOGD(TAG, "Safety edge: triggered -> handling once.");

    motor.setRun(false);
    motor.enableDriverForMovement();

    // Speed and torque for the backtrack.
    motor.setSpeed(static_cast<float>(adv.spdHiC()));
    motor.setTorque(adv.tqOps());

    applyBacktrackDirection(interrupted, movingOpen);

    if (doBacktrack) {
        ESP_LOGD(TAG, "Backtrack: pulsing start...");
        motor.setRun(true);
        sleepMs(adv.btTime());
        motor.setRun(false);
    }
}
