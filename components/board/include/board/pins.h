#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include "soc/gpio_num.h"

/**
 * @brief GPIO numbers of the Kopal gate controller board (was the pin constants in Program).
 */
namespace pins {

// Controller inputs
constexpr gpio_num_t kSafetyEdge = GPIO_NUM_1;       ///< was SAFETY_EDGE_PIN
constexpr gpio_num_t kPhotocell = GPIO_NUM_48;       ///< was PHOTOCELL_PIN
constexpr gpio_num_t kLso = GPIO_NUM_8;              ///< limit switch open (was LSO_PIN)
constexpr gpio_num_t kLsc = GPIO_NUM_9;              ///< limit switch closed (was LSC_PIN)
constexpr gpio_num_t kOpen = GPIO_NUM_10;            ///< was OPEN_PIN
constexpr gpio_num_t kOsc = GPIO_NUM_11;             ///< open-stop-close button (was OSC_PIN)
constexpr gpio_num_t kClose = GPIO_NUM_12;           ///< was CLOSE_PIN
constexpr gpio_num_t kPedestrian = GPIO_NUM_13;      ///< was PEDESTRIAN_PIN
constexpr gpio_num_t kAutoClose = GPIO_NUM_14;       ///< was AUTO_CLOSE_PIN
constexpr gpio_num_t kAuxIn = GPIO_NUM_35;           ///< was AUX_PIN_IN

// Controller outputs
constexpr gpio_num_t kPassageLight = GPIO_NUM_4;     ///< was PASSAGE_LIGHT_PIN
constexpr gpio_num_t kFlash = GPIO_NUM_5;            ///< beacon (was FLASH_PIN)
constexpr gpio_num_t kLock = GPIO_NUM_7;             ///< was LOCK_PIN
constexpr gpio_num_t kAuxOut = GPIO_NUM_6;           ///< was AUX_PIN_OUT
constexpr gpio_num_t kUserLed = GPIO_NUM_46;         ///< was USER_LED_PIN

// Motor driver: pins we drive
constexpr gpio_num_t kMotorDisable = GPIO_NUM_39;    ///< was MOTOR_DISABLE
constexpr gpio_num_t kMotorStartStop = GPIO_NUM_41;  ///< legacy wiring: run/stop (was MOTOR_START_STOP)
constexpr gpio_num_t kMotorDirection = GPIO_NUM_40;  ///< legacy wiring: direction (was MOTOR_DIRECTION)
constexpr gpio_num_t kMotorOpen = GPIO_NUM_41;       ///< dual wiring: run open, same pin as kMotorStartStop (was MOTOR_OPEN_PIN)
constexpr gpio_num_t kMotorClose = GPIO_NUM_40;      ///< dual wiring: run close, same pin as kMotorDirection (was MOTOR_CLOSE_PIN)
constexpr gpio_num_t kMotorSpeed = GPIO_NUM_38;      ///< speed PWM (was MOTOR_SPEED_IN)
constexpr gpio_num_t kMotorTorque = GPIO_NUM_42;     ///< torque limit PWM (was MOTOR_TORQUE_IN)

// Motor driver: pins we read
constexpr gpio_num_t kTorqueLimit = GPIO_NUM_34;     ///< driver alarm, torque too high (was TORQUE_LIMIT)
constexpr gpio_num_t kFeedbackPulses = GPIO_NUM_36;  ///< encoder pulses (was FEEDBACK_MOTOR_PULSES)
constexpr gpio_num_t kFeedbackDirection = GPIO_NUM_37; ///< direction feedback (was FEEDBACK_MOTOR_DIRECTION)
constexpr gpio_num_t kHallA = kFeedbackPulses;       ///< quadrature encoder A, same pin (was HALL_A)
constexpr gpio_num_t kHallB = kFeedbackDirection;    ///< quadrature encoder B, same pin (was HALL_B)

// Connectivity
constexpr gpio_num_t kReset = GPIO_NUM_2;            ///< reset button (was RESET_PIN)
constexpr gpio_num_t kWifi = GPIO_NUM_3;             ///< Wi-Fi switch (was WIFI_PIN)

// RS485 / Modbus (UART2)
constexpr gpio_num_t kModbusTx = GPIO_NUM_19;        ///< was the number 19 in the Modbus init
constexpr gpio_num_t kModbusRx = GPIO_NUM_20;        ///< was the number 20 in the Modbus init
constexpr gpio_num_t kModbusDe = GPIO_NUM_47;        ///< RS485 driver enable (was MODBUS_DE_PIN)

} // namespace pins

#endif // BOARD_PINS_H
