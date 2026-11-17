#include "motor_control/move_trace.h"

#include <cstdio>

#include "motor_control/time_util.h"

void MoveTrace::reset()
{
    mu.lock();
    n = 0;
    startMs = nowMs();
    lastCheckMs = kNever;
    lastSampleMs = kNever;
    lastDuty = 0.0f;
    lastRamp = ' ';
    lastLso = false;
    lastLsc = false;
    mu.unlock();
}

// True when the buffer has room and kCheckMs has passed since the last record() call.
// The caller reads the pins only when this is true.
bool MoveTrace::due() const
{
    mu.lock();
    const bool isDue = n < kCapacity && nowMs() - lastCheckMs >= kCheckMs;
    mu.unlock();
    return isDue;
}

void MoveTrace::record(int64_t count, float duty, float rpm, char dir, char ramp, bool lso, bool lsc)
{
    mu.lock();
    if (n >= kCapacity) {
        mu.unlock();
        return;
    }

    const int64_t now = nowMs();
    lastCheckMs = now;   // restarts the kCheckMs window, whether or not a sample is stored

    float dDuty = duty - lastDuty;
    if (dDuty < 0) {
        dDuty = -dDuty;
    }

    const bool changed = n == 0 || dDuty >= kDutyStep || ramp != lastRamp || lso != lastLso || lsc != lastLsc;
    if (!changed && now - lastSampleMs < kPlateauMs) {
        mu.unlock();
        return;
    }

    lastSampleMs = now;
    lastDuty = duty;
    lastRamp = ramp;
    lastLso = lso;
    lastLsc = lsc;

    Sample& s = samples[n];
    s.tMs = static_cast<int32_t>(now - startMs);
    s.count = static_cast<int32_t>(count);
    s.duty = duty;
    s.rpm = rpm;
    s.dir = dir;
    s.ramp = ramp;
    s.lso = lso ? '1' : '0';
    s.lsc = lsc ? '1' : '0';
    ++n;
    mu.unlock();
}

void MoveTrace::write(const Sink& sink) const
{
    sink("t_ms,dir,cnt,duty,rpm,ramp,lso,lsc\r\n");

    mu.lock();
    char line[80];
    for (int i = 0; i < n; ++i) {
        const Sample& s = samples[i];
        snprintf(line, sizeof(line), "%ld,%c,%ld,%.2f,%.0f,%c,%c,%c\r\n",
                 static_cast<long>(s.tMs), s.dir, static_cast<long>(s.count),
                 s.duty, s.rpm, s.ramp, s.lso, s.lsc);
        sink(line);
    }
    mu.unlock();
}
