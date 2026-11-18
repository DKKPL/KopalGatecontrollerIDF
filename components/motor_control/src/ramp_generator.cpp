#include "motor_control/ramp_generator.h"

#include "hal_motor/hal_motor.h"
#include "motor_control/time_util.h"

RampGenerator::RampGenerator(Kind kind, double startValue, double stopValue, double periodS, int64_t startMs)
{
    this->kind = kind;
    this->startValue = startValue;
    this->stopValue = stopValue;
    this->periodS = periodS;
    this->startMs = startMs;
}

double RampGenerator::currentValue(int64_t nowMs)
{
    if (done) {
        return stopValue;
    }

    const double elapsedS = static_cast<double>(nowMs - startMs) / 1000.0;
    const double value = calculate(elapsedS);
    const bool reached = (kind == Kind::Up) ? value >= stopValue : value <= stopValue;

    if (elapsedS >= duration() || reached) {
        done = true;
        return stopValue;
    }
    return value;
}

// Guards against a zero ramp period, which would otherwise jump straight to the end value.
double RampGenerator::duration() const
{
    return periodS == 0.0 ? 0.5 : periodS;
}

double RampGenerator::calculate(double elapsedS) const
{
    const double progress = elapsedS / duration();
    return startValue + (stopValue - startValue) * progress;
}

SpeedRamp::SpeedRamp(Motor& motor) : motor(motor)
{
}

void SpeedRamp::clear()
{
    ramp.reset();
    finished = false;
}

bool SpeedRamp::rampingUp() const
{
    return ramp && ramp->getKind() == RampGenerator::Kind::Up;
}

bool SpeedRamp::rampingDown() const
{
    return ramp && ramp->getKind() == RampGenerator::Kind::Down;
}

void SpeedRamp::tryStartDown(double stopValue, double decPeriod)
{
    if (rampingDown()) {
        return;
    }

    double startValue = motor.getSpeed();
    if (startValue < stopValue) {
        startValue = stopValue;
    }
    constexpr double kMaxSpeed = 1.0;   // always reference max speed
    const double decelRate = decPeriod > 0.0 ? kMaxSpeed / decPeriod : 0.0;
    double adjustedDecPeriod = decelRate > 0.0 ? (startValue - stopValue) / decelRate : 0.0;
    if (adjustedDecPeriod < 0.0) {
        adjustedDecPeriod = 0.0;
    }
    //this builds an object in the optional container
    ramp.emplace(RampGenerator::Kind::Down, startValue, stopValue, adjustedDecPeriod, nowMs());
    finished = false;
    lastStepMs = 0;
    tick();
}

void SpeedRamp::tryStartUp(double targetValue, double accPeriod)
{
    if (rampingUp()) {
        return;
    }

    motor.setSpeed(0.0f);
    double startValue = 0.0;   // C# read back the duty it had just set to 0
    if (startValue > targetValue) {
        startValue = targetValue;
    }
    constexpr double kMaxSpeed = 1.0;
    const double accelRate = accPeriod > 0.0 ? kMaxSpeed / accPeriod : 0.0;
    double adjustedAccPeriod = accelRate > 0.0 ? (targetValue - startValue) / accelRate : 0.0;
    if (adjustedAccPeriod < 0.0) {
        adjustedAccPeriod = 0.0;
    }
    //this builds an object in the optional container
    ramp.emplace(RampGenerator::Kind::Up, startValue, targetValue, adjustedAccPeriod, nowMs());
    finished = false;
    lastStepMs = 0;
    tick();
}

void SpeedRamp::tick()
{
    if (!ramp || finished) {
        return;
    }

    const int64_t now = nowMs();
    if (now - lastStepMs < kStepMs) {
        return;
    }
    lastStepMs = now;

    const double value = ramp->currentValue(now);
    motor.setSpeed(static_cast<float>(value));
    if (value == ramp->getStopValue()) {
        finished = true;   // the ramp object stays, like the C# generator field did
    }
}
