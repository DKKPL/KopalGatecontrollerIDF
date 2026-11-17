#ifndef HAL_MOTOR_H
#define HAL_MOTOR_H

#include <cstdint>

// Logical gate direction. hal_motor maps it to the driver pins
// (including INVERT_MOTOR_DIRECTION and the dual-input wiring).
enum class Direction : uint8_t {
    Close = 0,   // was CloseDirection = PinValue.Low
    Open = 1,    // was OpenDirection  = PinValue.High
};

#endif // HAL_MOTOR_H
