#ifndef MOTOR_CONTROL_TIME_UTIL_H
#define MOTOR_CONTROL_TIME_UTIL_H

#include <cstdint>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// PollingDelayMs
constexpr int kPollIntervalMs = 10;

// DateTime.UtcNow -> milliseconds since boot
inline int64_t nowMs()
{
    return esp_timer_get_time() / 1000;
}

// (DateTime.UtcNow - since).TotalSeconds
inline double secondsSince(int64_t sinceMs)
{
    return static_cast<double>(nowMs() - sinceMs) / 1000.0;
}

// Sleep(ms) -> Thread.Sleep(ms)
inline void sleepMs(int ms)
{
    if (ms <= 0) {
        return;
    }
    const TickType_t ticks = pdMS_TO_TICKS(ms);
    vTaskDelay(ticks > 0 ? ticks : 1);
}

#endif // MOTOR_CONTROL_TIME_UTIL_H
