#ifndef MOTOR_CONTROL_TIME_UTIL_H
#define MOTOR_CONTROL_TIME_UTIL_H

#include <cstdint>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief How often the gate loop runs, in milliseconds (was PollingDelayMs).
 */
constexpr int kPollIntervalMs = 10;

/**
 * @brief Milliseconds since the chip started. All timing uses this (was DateTime.UtcNow).
 */
inline int64_t nowMs()
{
    return esp_timer_get_time() / 1000;
}

/**
 * @brief Seconds that have passed since sinceMs (a value you got from nowMs()).
 */
inline double secondsSince(int64_t sinceMs)
{
    return static_cast<double>(nowMs() - sinceMs) / 1000.0;
}

/**
 * @brief Pause this task for ms milliseconds; other tasks keep running (was Sleep).
 */
inline void sleepMs(int ms)
{
    if (ms <= 0) {
        return;
    }
    const TickType_t ticks = pdMS_TO_TICKS(ms);
    vTaskDelay(ticks > 0 ? ticks : 1);
}

#endif // MOTOR_CONTROL_TIME_UTIL_H
