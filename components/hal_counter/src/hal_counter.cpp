#include "hal_counter/hal_counter.h"

#include "board/pins.h"
#include "driver/pulse_cnt.h"
#include "esp_err.h"

// Shortest pulse that counts; shorter ones are noise. The C# name says µs, but
// nanoFramework passes the value to the driver as max_glitch_ns, so it is 1023 ns.
static constexpr uint32_t kGlitchFilterNs = 1023;   // was ENCODER_FILTER_MIN_US

// The PCNT counter is 16 bit. At these limits the driver adds the count to a running
// total (nanoFramework did the same with its own overflow callback).
static constexpr int kCountLimit = 0x7fff;

void Counter::begin(bool useQuadratureEncoder)
{
    pcnt_unit_config_t unitConfig = {};
    unitConfig.low_limit = -kCountLimit;
    unitConfig.high_limit = kCountLimit;
    unitConfig.flags.accum_count = 1;
    ESP_ERROR_CHECK(pcnt_new_unit(&unitConfig, &unit));

    // Channel A: rising edges of Hall A (Polarity = Rising).
    pcnt_chan_config_t configA = {};
    configA.edge_gpio_num = useQuadratureEncoder ? pins::kHallA : pins::kFeedbackPulses;
    configA.level_gpio_num = useQuadratureEncoder ? pins::kHallB : -1;
    ESP_ERROR_CHECK(pcnt_new_channel(unit, &configA, &channelA));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(channelA, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                                 PCNT_CHANNEL_EDGE_ACTION_HOLD));

    if (useQuadratureEncoder) {
        // Hall B low reverses channel A.
        ESP_ERROR_CHECK(pcnt_channel_set_level_action(channelA, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                      PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

        // Channel B: both edges of Hall B; Hall A low reverses it.
        pcnt_chan_config_t configB = {};
        configB.edge_gpio_num = pins::kHallB;
        configB.level_gpio_num = pins::kHallA;
        ESP_ERROR_CHECK(pcnt_new_channel(unit, &configB, &channelB));
        ESP_ERROR_CHECK(pcnt_channel_set_edge_action(channelB, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                                     PCNT_CHANNEL_EDGE_ACTION_DECREASE));
        ESP_ERROR_CHECK(pcnt_channel_set_level_action(channelB, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                      PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    } else {
        ESP_ERROR_CHECK(pcnt_channel_set_level_action(channelA, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                      PCNT_CHANNEL_LEVEL_ACTION_KEEP));
    }

    // Ignore pulses shorter than the filter (was FilterPulses = ENCODER_FILTER_MIN_US).
    pcnt_glitch_filter_config_t filter = {};
    filter.max_glitch_ns = kGlitchFilterNs;
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(unit, &filter));

    // accum_count needs a watch point at each limit.
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(unit, kCountLimit));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(unit, -kCountLimit));

    ESP_ERROR_CHECK(pcnt_unit_enable(unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(unit));
    ESP_ERROR_CHECK(pcnt_unit_start(unit));
}

bool Counter::isStarted() const
{
    return unit != nullptr;
}

int32_t Counter::takeCount()
{
    if (unit == nullptr) {
        return 0;
    }
    int count = 0;
    pcnt_unit_get_count(unit, &count);
    pcnt_unit_clear_count(unit);
    return count;
}

void Counter::stop()
{
    if (unit == nullptr) {
        return;
    }
    pcnt_unit_stop(unit);
    pcnt_unit_disable(unit);
    pcnt_unit_remove_watch_point(unit, kCountLimit);
    pcnt_unit_remove_watch_point(unit, -kCountLimit);
    if (channelB != nullptr) {
        pcnt_del_channel(channelB);
        channelB = nullptr;
    }
    pcnt_del_channel(channelA);
    channelA = nullptr;
    pcnt_del_unit(unit);
    unit = nullptr;
}
