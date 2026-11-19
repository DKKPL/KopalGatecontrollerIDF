#ifndef APP_CONTROLLER_TEACH_H
#define APP_CONTROLLER_TEACH_H

#include "app_controller/gate_types.h"

class GateController;

/**
 * @brief Learns how far the gate travels ("teach"), and finds the gate position after power-up.
 */
class TeachSequence {
public:
    /**
     * @brief gate: the controller this sequence moves.
     */
    explicit TeachSequence(GateController& gate);

    /**
     * @brief Teach: close to the close limit, open to the open limit, save the open position.
     *        Skipped when the gate is already taught. Blocks until done (was State_TeachCode).
     */
    void run();
    /**
     * @brief After power-up: use the limit switches to know where the gate is,
     *        or close carefully until the close limit is reached (was ReestablishGatePosition).
     */
    void reestablishGatePosition();
    /**
     * @brief When the reset button is held at power-up, forget the taught positions
     *        so the gate teaches again (was State_CheckLearn).
     */
    void checkResetButton();

    /**
     * @brief The teach step that is running.
     */
    TeachPhase getPhase() const;

private:
    GateController& gate;
    TeachPhase phase = TeachPhase::CloseToLimit;   ///< _teachPhase
};

#endif // APP_CONTROLLER_TEACH_H
