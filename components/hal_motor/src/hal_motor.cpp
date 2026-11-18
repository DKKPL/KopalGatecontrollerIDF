#include "hal_motor/hal_motor.h"

#include "board/pins.h"
#include "config/advanced_settings.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "motor";

// Both PWM signals run at 16 kHz (was PwmChannel.CreateFromPin(pin, 16000)).
static constexpr uint32_t kPwmFrequencyHz = 16000;
static constexpr ledc_timer_bit_t kPwmResolution = LEDC_TIMER_12_BIT;
static constexpr uint32_t kDutyFull = 1u << 12;                    // 100 % duty at 12 bit
static constexpr ledc_channel_t kTorqueChannel = LEDC_CHANNEL_0;   // was PWM1
static constexpr ledc_channel_t kSpeedChannel = LEDC_CHANNEL_1;    // was PWM2

// Open one pin as output and set it low.
static void openOutput(gpio_num_t pin)
{
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << pin;
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(pin, 0);
}

// Connect one PWM channel to its pin, with duty 0.
static void openPwm(gpio_num_t pin, ledc_channel_t channel)
{
    ledc_channel_config_t cfg = {};
    cfg.gpio_num = pin;
    cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    cfg.channel = channel;
    cfg.timer_sel = LEDC_TIMER_0;
    cfg.duty = 0;
    cfg.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&cfg));
}

// Write a duty of 0..1 to a PWM channel.
static void writeDuty(ledc_channel_t channel, float duty)
{
    if (duty < 0.0f) {
        duty = 0.0f;
    }
    if (duty > 1.0f) {
        duty = 1.0f;
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, static_cast<uint32_t>(duty * kDutyFull + 0.5f));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
}

Motor::Motor(const AdvancedSettings& adv) : adv(adv)
{
}
