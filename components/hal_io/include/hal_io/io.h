#ifndef HAL_IO_H
#define HAL_IO_H

#include <cstdint>

/**
 * @brief A digital input of the controller board.
 */
enum class Input : uint8_t {
    Lsc,                ///< limit switch closed (was _LscPinIn)
    Lso,                ///< limit switch open (was _LsoPinIn)
    Photocell,          ///< was _PhotoCellPinIn
    SafetyEdge,         ///< was _SafetyEdgePinIn
    Osc,                ///< open-stop-close button (was _OscPinIn)
    Close,              ///< was _ClosePinIn
    Open,               ///< was _OpenPinIn
    Pedestrian,         ///< was _PedestrianPinIn
    AutoClose,          ///< was _AutoClosePinIn
    AuxIn,              ///< was _AuxPinIn
    TorqueLimit,        ///< driver alarm (was _torqueLimitPinIn)
    FeedbackDirection,  ///< direction feedback, only without quadrature encoder (was _feedbackDirectionPinIn)
    Wifi,               ///< Wi-Fi switch (was _wifiPinIn)
    Reset,              ///< reset button (was _ResetPinIn)
};

/**
 * @brief A digital output of the controller board.
 */
enum class Output : uint8_t {
    PassageLight,  ///< was _passageLightOut
    Flash,         ///< beacon (was _flashPinOut)
    Lock,          ///< was _lockPinOut
    AuxOut,        ///< was _AuxPinOut
    UserLed,       ///< was _UserLedPinOut
};

/**
 * @brief The digital inputs and outputs of the board (was the GpioPin fields in Program).
 */
class Io {
public:
    /**
     * @brief Open every input and output pin; outputs start low (was SetDefaultGPIOStateCode).
     * @param useQuadratureEncoder true: the direction feedback pin belongs to the encoder and stays closed
     */
    void begin(bool useQuadratureEncoder);

    /**
     * @brief Read an input now (was _XxxPinIn.Read() == PinValue.High).
     */
    bool isHigh(Input input) const;

    /**
     * @brief Set an output high or low (was _XxxPinOut.Write).
     */
    void write(Output output, bool high);

    /**
     * @brief Whether the direction feedback pin is in use (was _feedbackDirectionPinIn != null).
     */
    bool hasFeedbackDirection() const;

private:
    bool feedbackDirection = false;   ///< true when begin() opened the direction feedback pin
};

#endif // HAL_IO_H
