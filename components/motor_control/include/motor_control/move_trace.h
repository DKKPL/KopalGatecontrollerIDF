#ifndef MOTOR_CONTROL_MOVE_TRACE_H
#define MOTOR_CONTROL_MOVE_TRACE_H

#include <array>
#include <cstdint>
#include <functional>
#include <mutex>

/**
 * @brief Records speed, position and limit switches during one movement, for the /trace web page.
 */
class MoveTrace {
public:
    /**
     * @brief A function that receives the trace text piece by piece (see write()).
     */
    using Sink = std::function<void(const char* part)>;

    /**
     * @brief Empty the buffer and start timing a new movement.
     */
    void reset();
    /**
     * @brief True when it is time for the next sample (100 ms since the last one, and the buffer is not full).
     */
    bool due() const;
    /**
     * @brief Store a sample if something changed, or every 2 s when nothing changes.
     */
    void record(int64_t count, float duty, float rpm, char dir, char ramp, bool lso, bool lsc);
    /**
     * @brief Send all samples to sink as CSV text, one line per sample.
     */
    void write(const Sink& sink) const;

private:
//use constexp for values that do not change like limits, timings and sized
    static constexpr int kCapacity = 300;      ///< max samples per movement
    static constexpr int kCheckMs = 100;       ///< min time between two record() calls
    static constexpr int kPlateauMs = 2000;    ///< max time between stored samples when nothing changes
    static constexpr float kDutyStep = 0.02f;  ///< a speed change this big always stores a sample
    static constexpr int64_t kNever = INT64_MIN / 2;

    /**
     * @brief One stored sample.
     */
    struct Sample {
        int32_t tMs;    ///< time since reset()
        int32_t count;  ///< gate position in pulses
        float duty;     ///< speed output 0..1
        float rpm;
        char dir;       ///< 'O' opening, 'C' closing
        char ramp;      ///< 'U' up, 'D' down, '-' none
        char lso;       ///< open limit '1'/'0'
        char lsc;       ///< close limit '1'/'0'
    };
    //mutex so two threads don't use the buffer at the samettime. 
    mutable std::mutex mu;   ///< read only funcitons can take the lock too (because of mutable)
    std::array<Sample, kCapacity> samples{};
    int n = 0;
    int64_t startMs = 0;
    int64_t lastCheckMs = kNever;
    int64_t lastSampleMs = kNever;
    float lastDuty = 0.0f;
    char lastRamp = ' ';
    bool lastLso = false;
    bool lastLsc = false;
};

#endif // MOTOR_CONTROL_MOVE_TRACE_H
