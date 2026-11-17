#ifndef HAL_COUNTER_H
#define HAL_COUNTER_H

#include <cstdint>

struct pcnt_unit_t;
struct pcnt_chan_t;

/**
 * @brief Counts the motor encoder pulses with the PCNT peripheral (was GpioPulseCounter pulseCounter).
 */
class Counter {
public:
    /**
     * @brief Create and start the counter (was new GpioPulseCounter(...) and Start()).
     * @param useQuadratureEncoder true: Hall A + B, counts up and down; false: Hall A only, counts up
     */
    void begin(bool useQuadratureEncoder);

    /**
     * @brief Whether begin() ran and stop() did not (was pulseCounter != null).
     */
    bool isStarted() const;

    /**
     * @brief The pulses since the last call; sets the count back to 0 (was pulseCounter.Reset().Count).
     */
    int32_t takeCount();

    /**
     * @brief Stop and delete the counter; the next begin() creates it again (was ResetPulseCounter).
     */
    void stop();

private:
    pcnt_unit_t* unit = nullptr;       ///< PCNT unit, nullptr when not started
    pcnt_chan_t* channelA = nullptr;   ///< counts the edges of Hall A
    pcnt_chan_t* channelB = nullptr;   ///< counts the edges of Hall B (quadrature only)
};

#endif // HAL_COUNTER_H
