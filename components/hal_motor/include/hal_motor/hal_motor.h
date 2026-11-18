#ifndef HAL_MOTOR_H
#define HAL_MOTOR_H

#include <cstdint>

class AdvancedSettings;

/**
 * @brief Logical gate direction. hal_motor maps it to the driver pins
 *        (including INVERT_MOTOR_DIRECTION and the dual-input wiring).
 */
enum class Direction : uint8_t {
    Close = 0,   ///< was CloseDirection = PinValue.Low
    Open = 1,    ///< was OpenDirection  = PinValue.High
};

/**
 * @brief The motor driver: run and direction pins, disable pin, speed and torque PWM.
 */
class Motor {
public:
    /**
     * @brief Keep the settings; the pins are set up in begin().
     */
    explicit Motor(const AdvancedSettings& adv);

    /**
     * @brief Set up the driver pins and both PWM signals (was the motor part of SetDefaultGPIOStateCode).
     */
    void begin();

    /**
     * @brief Set the direction to drive in (was WriteMotorDirection).
     */
    void setDirection(Direction direction);

    /**
     * @brief Start or stop the motor (was WriteMotorRun).
     */
    void setRun(bool run);

    /**
     * @brief Switch to the other direction (was ToggleMotorDirection).
     */
    void toggleDirection();

    /**
     * @brief Set the speed PWM duty, 0..1 (was _SpeedPwm.DutyCycle = x and Start()).
     */
    void setSpeed(float duty);

    /**
     * @brief The speed duty set last (was _SpeedPwm.DutyCycle).
     */
    float getSpeed() const;

    /**
     * @brief Set the torque limit PWM duty, 0..1 (was _torquePwm.DutyCycle = x and Start()).
     */
    void setTorque(float duty);

    /**
     * @brief The torque duty set last (was _torquePwm.DutyCycle).
     */
    float getTorque() const;

    /**
     * @brief Drive the driver's disable pin directly (was _motorDisablePinOut.Write).
     */
    void setDriverDisabled(bool disabled);

    /**
     * @brief Set the disable pin as the USE_MOTOR_DISABLE setting says (was ApplyUseMotorDisableSetting).
     */
    void applyUseMotorDisableSetting();

    /**
     * @brief Enable the driver before a move, unless USE_MOTOR_DISABLE is on (was EnsureMotorDriverEnabledForMovement).
     */
    void enableDriverForMovement();

private:
    /**
     * @brief Write run and direction to the two driver pins (was ApplyMotorOutputs).
     */
    void applyOutputs();

    const AdvancedSettings& adv;
    bool run = false;                         ///< was _motorRunLogical
    Direction direction = Direction::Close;   ///< was _motorDirectionLogical
    float speedDuty = 0.0f;                   ///< was _SpeedPwm.DutyCycle
    float torqueDuty = 0.0f;                  ///< was _torquePwm.DutyCycle
};

#endif // HAL_MOTOR_H
