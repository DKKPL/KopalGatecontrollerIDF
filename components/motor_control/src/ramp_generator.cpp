#include "motor_control/ramp_generator.h"

#include "hal_motor/motor.h"
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
