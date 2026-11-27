#ifndef APP_CONTROLLER_AUTO_CLOSE_TIMER_H
#define APP_CONTROLLER_AUTO_CLOSE_TIMER_H

#include <cstdint>

class AutoCloseTimer {
public:
    void update(bool enabled, bool blocked, int64_t now)
    {
        if (!enabled || !wasEnabled || blocked || wasBlocked) {
            sinceMs = now;
        }
        wasEnabled = enabled;
        wasBlocked = blocked;
    }

    void reset(int64_t now)
    {
        sinceMs = now;
    }

    bool elapsed(int64_t now, int delaySeconds) const
    {
        return now - sinceMs > static_cast<int64_t>(delaySeconds) * 1000;
    }

private:
    int64_t sinceMs = 0;
    bool wasEnabled = false;
    bool wasBlocked = false;
};

#endif // APP_CONTROLLER_AUTO_CLOSE_TIMER_H
