#ifndef MOTOR_CONTROL_RAMP_GENERATOR_H
#define MOTOR_CONTROL_RAMP_GENERATOR_H

#include <cstdint>
#include <optional>

class Motor;

// How the two classes below work together:
//   RampGenerator = the plan for one speed change ("0 -> 0.8 in 1.6 s").
//                   Pure maths: currentValue(t) says what the speed should be at time t.
//                   It never touches the motor.
//   SpeedRamp     = carries out the plan. Holds at most one RampGenerator, starts a new one
//                   with tryStartUp()/tryStartDown() (called by GateController::setMotorSpeed),
//                   and every 20 ms (tick()) writes the planned speed to the motor.

/**
 * @brief One straight-line speed change: from a start speed to an end speed in a set time.
 *        (Was RampUpGenerator / RampDownGenerator.)
 */
class RampGenerator {
public:
    /**
     * @brief Up = the speed rises, Down = the speed falls.
     */
    enum class Kind : uint8_t { Up, Down };

    /**
     * @brief A ramp that starts at time startMs and goes from startValue to stopValue in periodS seconds.
     */
    RampGenerator(Kind kind, double startValue, double stopValue, double periodS, int64_t startMs);

    /**
     * @brief The speed the ramp wants at time nowMs; stays at the end speed once it is reached.
     */
    double currentValue(int64_t nowMs);
    /**
     * @brief The speed the ramp ends at.
     */
    double getStopValue() const;
    /**
     * @brief Whether this ramp goes up or down.
     */
    Kind getKind() const;

private:
    /**
     * @brief Ramp time in seconds; 0.5 s when the setting is 0, so the speed never jumps.
     */
    double duration() const;
    /**
     * @brief The speed elapsedS seconds after the start (currentValue() stops it at the end speed).
     */
    double calculate(double elapsedS) const;

    Kind kind;
    double startValue;
    double stopValue;
    double periodS;
    int64_t startMs;
    bool done = false;   ///< true once the end speed is reached
};

/**
 * @brief Drives the motor speed through at most one ramp at a time.
 *        The gate loop calls tick() every pass; a new speed is written every 20 ms.
 */
class SpeedRamp {
public:
    /**
     * @brief The ramps write their speed to this motor.
     */
    explicit SpeedRamp(Motor& motor);

    /**
     * @brief Stop ramping. The motor keeps its current speed.
     */
    void clear();
    /**
     * @brief Slow down from the current speed to stopValue. decPeriod = seconds from full speed to 0.
     *        Does nothing if a slow-down is already running.
     */
    void tryStartDown(double stopValue, double decPeriod);
    /**
     * @brief Speed up from 0 to targetValue. accPeriod = seconds from 0 to full speed.
     *        Does nothing if a speed-up is already running.
     */
    void tryStartUp(double targetValue, double accPeriod);
    /**
     * @brief Call every loop: writes the next speed step to the motor (every 20 ms).
     */
    void tick();

    /**
     * @brief True when the last ramp started was a speed-up (also after it finished, until clear()).
     */
    bool rampingUp() const;
    /**
     * @brief True when the last ramp started was a slow-down (also after it finished, until clear()).
     */
    bool rampingDown() const;

private:
    static constexpr int kStepMs = 20;   ///< time between two speed writes

    Motor& motor;
    std::optional<RampGenerator> ramp;   ///< the active ramp, if any
    bool finished = false;               ///< the ramp reached its end speed
    int64_t lastStepMs = 0;              ///< when the speed was last written
};

#endif // MOTOR_CONTROL_RAMP_GENERATOR_H
