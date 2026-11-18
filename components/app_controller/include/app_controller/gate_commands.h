#ifndef APP_CONTROLLER_GATE_COMMANDS_H
#define APP_CONTROLLER_GATE_COMMANDS_H

#include "app_controller/gate_types.h"

class Io;

/**
 * @brief What someone asks the gate to do: the wall buttons, the auto-close switch and
 *        remote commands from Modbus or the web page. A press is remembered ("latched")
 *        until the state machine has handled it.
 */
class GateCommands {
public:
    /**
     * @brief io: where the buttons are read.
     */
    explicit GateCommands(const Io& io);

    /**
     * @brief Read the OSC button (open / stop / close); a new press sets latchedOsc. Ignored while Open is held.
     */
    void pollOsc();
    /**
     * @brief Read the Close button; true while it is held (Open wins when both are held).
     */
    bool pollClose();
    /**
     * @brief Read the Open button; true while it is held.
     */
    bool pollOpen();
    /**
     * @brief Read the Pedestrian button; a new press sets pedestrianPressed, holding it sets pedestrianHold.
     */
    void pollPedestrian();
    /**
     * @brief Read the auto-close switch; true when auto-close is on.
     */
    bool pollAutoClose();
    /**
     * @brief Handle one command from Modbus or the web page as if the button was pressed.
     */
    void applyCommand(GateCommand cmd);
    /**
     * @brief Forget all remembered presses.
     */
    void clearLatches();

    // Read and cleared by the gate state functions (1:1 with the C# flags).
    bool openPressed = false;        ///< Open held now (bOpenPressed)
    bool closePressed = false;       ///< Close held now (bClosePressed)
    bool pedestrianPressed = false;  ///< Pedestrian pressed, not handled yet (bPedestrianPressed)
    bool pedestrianHold = false;     ///< Pedestrian held now (bPedestrianHold)
    bool autoClose = false;          ///< auto-close switch on (bAutoClose)
    bool latchedOsc = false;         ///< OSC pressed, not handled yet (bLatchedOSCPressed)
    bool latchedOpen = false;        ///< Open pressed, not handled yet (bLatchedOpenPressed)
    bool latchedClose = false;       ///< Close pressed, not handled yet (bLatchedClosePressed)
    bool latchedPedestrian = false;  ///< Pedestrian latched while waiting (bLatchedPedestrianPressed)

private:
    const Io& io;
    bool lastOscHigh = true;          ///< OSC level at the last poll (_lastOscPinValue)
    bool lastPedestrianHigh = true;   ///< Pedestrian level at the last poll (_lastPedestrianPinValue)
};

#endif // APP_CONTROLLER_GATE_COMMANDS_H
