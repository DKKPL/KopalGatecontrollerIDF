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

void Motor::begin()
{
    // Driver disable pin, as the USE_MOTOR_DISABLE setting says.
    openOutput(pins::kMotorDisable);
    applyUseMotorDisableSetting();

    // Run/stop and direction pins (open and close enable with the dual wiring), both low.
    openOutput(pins::kMotorStartStop);
    openOutput(pins::kMotorDirection);

    // One 16 kHz timer for the torque and speed PWM.
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = kPwmResolution;
    timer.timer_num = LEDC_TIMER_0;
    timer.freq_hz = kPwmFrequencyHz;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    openPwm(pins::kMotorTorque, kTorqueChannel);
    openPwm(pins::kMotorSpeed, kSpeedChannel);
}

void Motor::setDirection(Direction direction)
{
    this->direction = direction;
    applyOutputs();
}

void Motor::setRun(bool run)
{
    this->run = run;
    applyOutputs();
}

void Motor::toggleDirection()
{
    direction = (direction == Direction::Open) ? Direction::Close : Direction::Open;
    applyOutputs();
}

// Drives the two driver pins according to the wiring:
//   USE_DUAL_DIRECTION_INPUTS == false (legacy): start/stop pin = run/stop, direction pin = direction
//   USE_DUAL_DIRECTION_INPUTS == true  (dual)  : start/stop pin = open enable, direction pin = close enable,
//                                                break-before-make: never both high at once.
void Motor::applyOutputs()
{
    // INVERT_MOTOR_DIRECTION flips the physical direction when the motor is wired or geared
    // the other way round. Done here, the single logical -> physical point, so every caller is covered.
    Direction effective = direction;
    if (adv.invertMotorDirection()) {
        effective = (direction == Direction::Open) ? Direction::Close : Direction::Open;
    }
    // Inputs for the motor
    if (!adv.useDualDirectionInputs()) {
        // Legacy behaviour, exactly as before.
        gpio_set_level(pins::kMotorStartStop, run ? 1 : 0);
        gpio_set_level(pins::kMotorDirection, effective == Direction::Open ? 1 : 0);
        return;
    }

    // Dual direction-enable pins:
    //   kMotorStartStop   kMotorDirection   Motor
    //   low               low               stopped
    //   high              low               runs open
    //   low               high              runs close
    if (!run) {
        gpio_set_level(pins::kMotorStartStop, 0);
        gpio_set_level(pins::kMotorDirection, 0);
        return;
    }

    // Break-before-make with dead time: both lines low first, so there is a real
    // off period on the hardware before the target enable line goes high.
    gpio_set_level(pins::kMotorStartStop, 0);
    gpio_set_level(pins::kMotorDirection, 0);
    vTaskDelay(pdMS_TO_TICKS(5));

    if (effective == Direction::Open) {
        gpio_set_level(pins::kMotorStartStop, 1);
    } else {
        gpio_set_level(pins::kMotorDirection, 1);
    }
}

void Motor::setSpeed(float duty)
{
    speedDuty = duty;
    writeDuty(kSpeedChannel, duty);
}

float Motor::getSpeed() const
{
    return speedDuty;
}

void Motor::setTorque(float duty)
{
    torqueDuty = duty;
    writeDuty(kTorqueChannel, duty);
}

float Motor::getTorque() const
{
    return torqueDuty;
}
