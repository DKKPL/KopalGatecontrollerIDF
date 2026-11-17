#ifndef MOTOR_CONTROL_RAMP_GENERATOR_H
#define MOTOR_CONTROL_RAMP_GENERATOR_H

#include <cstdint>
#include <optional>

class Motor;

// One linear speed ramp (was RampUpGenerator / RampDownGenerator).
class RampGenerator {
public:
    enum class Kind : uint8_t { Up, Down };

    RampGenerator(Kind kind, double startValue, double stopValue, double periodS, int64_t startMs);

    double currentValue(int64_t nowMs);   // GetCurrentValue
    double getStopValue() const { return stopValue; }
    Kind getKind() const { return kind; }

private:
    double duration() const;                     // GetDuration
    double calculate(double elapsedS) const;     // CalculateCurrentValue

    Kind kind;
    double startValue;
    double stopValue;
    double periodS;
    int64_t startMs;
    bool done = false;                          // _isStopped / _isCompleted
};

// The active ramp and the speed output it drives.
class SpeedRamp {
public:
    explicit SpeedRamp(Motor& motor);

    void clear();                                            // ClearRamps
    void tryStartDown(double stopValue, double decPeriod);   // TryStartRampDown
    void tryStartUp(double targetValue, double accPeriod);   // TryStartRampUp
    void tick();                                             // RampUpThread / RampDownThread

    bool rampingUp() const;     // rampUpGenerator != null
    bool rampingDown() const;   // rampDownGenerator != null

private:
    static constexpr int kStepMs = 20;

    Motor& motor;
    std::optional<RampGenerator> ramp;
    bool finished = false;
    int64_t lastStepMs = 0;
};

#endif // MOTOR_CONTROL_RAMP_GENERATOR_H
