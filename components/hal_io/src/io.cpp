#include "hal_io/io.h"

#include "board/pins.h"
#include "driver/gpio.h"
#include "esp_err.h"

// Open one pin as input, with or without the internal pull-up.
static void openInput(gpio_num_t pin, bool pullUp)
{
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << pin;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = pullUp ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

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

// The GPIO behind each input.
static gpio_num_t inputPin(Input input)
{
    switch (input) {
    case Input::Lsc:               return pins::kLsc;
    case Input::Lso:               return pins::kLso;
    case Input::Photocell:         return pins::kPhotocell;
    case Input::SafetyEdge:        return pins::kSafetyEdge;
    case Input::Osc:               return pins::kOsc;
    case Input::Close:             return pins::kClose;
    case Input::Open:              return pins::kOpen;
    case Input::Pedestrian:        return pins::kPedestrian;
    case Input::AutoClose:         return pins::kAutoClose;
    case Input::AuxIn:             return pins::kAuxIn;
    case Input::TorqueLimit:       return pins::kTorqueLimit;
    case Input::FeedbackDirection: return pins::kFeedbackDirection;
    case Input::Wifi:              return pins::kWifi;
    case Input::Reset:             return pins::kReset;
    }
    return GPIO_NUM_NC;
}

// The GPIO behind each output.
static gpio_num_t outputPin(Output output)
{
    switch (output) {
    case Output::PassageLight: return pins::kPassageLight;
    case Output::Flash:        return pins::kFlash;
    case Output::Lock:         return pins::kLock;
    case Output::AuxOut:       return pins::kAuxOut;
    case Output::UserLed:      return pins::kUserLed;
    }
    return GPIO_NUM_NC;
}
