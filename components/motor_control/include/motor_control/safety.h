#ifndef MOTOR_CONTROL_SAFETY_H
#define MOTOR_CONTROL_SAFETY_H

#include <cstdint>

class Io;
class Motor;
class AdvancedSettings;
enum class Direction : uint8_t;

/**
 * @brief Which movement a safety trip interrupted, as far as the state machine knows.
 *        (The C# read Program's MainLoopStep here; motor_control cannot see the
 *        state machine, so app_controller passes this in.)
 */
enum class Interrupted : uint8_t {
    Unknown,   ///< not in a safety state: use the feedback pin or the last direction
    Opening,   ///< SafetyOpenAfterOpening / SafetyOpenCloseAfterOpening
    Closing,   ///< SafetyCloseAfterClosing / SafetyOpenCloseAfterClosing
};

/**
 * @brief Reads the safety inputs (photocell, safety edge, torque limit) and remembers which one tripped.
 */
class SafetyMonitor {
public:
    /**
     * @brief io: where the inputs are read. adv: settings (safety-edge wait time).
     */
    SafetyMonitor(const Io& io, const AdvancedSettings& adv);

    /**
     * @brief True when the gate may close: photocell and torque limit are clear (was SF_C).
     */
    bool closeOk() const;
    /**
     * @brief True when the gate may open: the torque limit is clear (was SF_O).
     */
    bool openOk() const;
    /**
     * @brief True when the safety edge is clear; a pressed edge blocks both directions (was SF_O_C).
     */
    bool edgeOk() const;
    /**
     * @brief True when every safety is clear (was IsSafetyOk).
     */
    bool allOk() const;

    /**
     * @brief Read the photocell now; true when the beam is blocked (was IsPhotoCellTriggered).
     */
    bool pollPhotocell();
    /**
     * @brief Read the motor driver's torque alarm now; true when it is on (was IsTorqueLimitTriggered).
     */
    bool pollTorqueLimit();
    /**
     * @brief Read the safety edge now; true when pressed, and it stays true until the edge has been
     *        released for SafetyEdgeWaitTime seconds (was IsSafetyEdgeTriggered).
     */
    bool pollSafetyEdge();

    /**
     * @brief Result of the last pollPhotocell().
     */
    bool isPhotocellTriggered() const;
    /**
     * @brief Result of the last pollTorqueLimit().
     */
    bool isTorqueLimitTriggered() const;
    /**
     * @brief Result of the last pollSafetyEdge().
     */
    bool isSafetyEdgeTriggered() const;

private:
    /**
     * @brief Start timing how long the safety edge has been released (if not timing yet).
     */
    void startEdgeClearTimer();
    /**
     * @brief Stop timing.
     */
    void stopEdgeClearTimer();

    const Io& io;
    const AdvancedSettings& adv;
    bool safetyEdgeTriggered = false;
    bool photocellTriggered = false;
    bool torqueLimitTriggered = false;
    int64_t edgeOkSinceMs = -1;  ///< when the edge was released; -1 = not timing
};

/**
 * @brief What the motor does after a safety trip: clear the torque alarm and back off a little.
 */
class SafetyActions {
public:
    /**
     * @brief motor: what is driven. io/monitor: input state. adv: settings (backtrack time, wiring).
     */
    SafetyActions(Motor& motor, const Io& io, SafetyMonitor& monitor, const AdvancedSettings& adv);

    /**
     * @brief Which way to back off: the opposite of the movement that was interrupted.
     *        movingOpen = the last known travel direction, used when nothing better is known.
     */
    Direction determineBacktrackDirection(Interrupted interrupted, bool movingOpen) const;
    /**
     * @brief Point the motor in the back-off direction, wait postWriteDelayMs, and return that direction.
     */
    Direction applyBacktrackDirection(Interrupted interrupted, bool movingOpen, int postWriteDelayMs = 200);
    /**
     * @brief If the torque alarm is on: toggle the driver until it clears, then back off for BT_TIME
     *        when doBacktrack is true. Blocks while it works.
     */
    void resetTorqueLimitIfTriggered(bool doBacktrack, Interrupted interrupted, bool movingOpen);
    /**
     * @brief Once per safety-edge press: stop the motor and, when doBacktrack is true, back off for BT_TIME.
     */
    void handleSafetyEdgeIfNeeded(bool doBacktrack, Interrupted interrupted, bool movingOpen);

private:
    Motor& motor;
    const Io& io;
    SafetyMonitor& monitor;
    const AdvancedSettings& adv;
    bool safetyEdgeHandled = false;  ///< this press was already handled
};

#endif // MOTOR_CONTROL_SAFETY_H
