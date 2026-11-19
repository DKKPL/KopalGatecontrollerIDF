#ifndef APP_CONTROLLER_GATE_OUTPUTS_H
#define APP_CONTROLLER_GATE_OUTPUTS_H

class Io;
class AdvancedSettings;

/**
 * @brief The lock, the warning beacon (flash) and the passage light.
 */
class GateOutputs {
public:
    /**
     * @brief io: where the outputs are written. adv: settings (solenoid or magnet lock).
     */
    GateOutputs(Io& io, const AdvancedSettings& adv);

    /**
     * @brief Lock the gate and switch the beacon off.
     */
    void engageLock();
    /**
     * @brief Unlock the gate and switch the beacon on, then wait preWarningMs before returning (0 = no wait).
     */
    void disengageLock(int preWarningMs);
    /**
     * @brief Switch the passage light on.
     */
    void passageLightOn();
    /**
     * @brief Switch the passage light off.
     */
    void passageLightOff();

    /**
     * @brief True while the gate is locked.
     */
    bool isLocked() const;
    /**
     * @brief True while the beacon is on.
     */
    bool isFlashOn() const;
    /**
     * @brief True while the passage light is on.
     */
    bool isPassageLightOn() const;

private:
    Io& io;
    const AdvancedSettings& adv;
    bool locked = true;         ///< bLocked
    bool passageLight = false;  ///< bPassageLight
    bool flash = false;         ///< bFlash
};

#endif // APP_CONTROLLER_GATE_OUTPUTS_H
