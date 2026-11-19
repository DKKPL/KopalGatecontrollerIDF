#include "app_controller/gate_outputs.h"

#include "config/advanced_settings.h"
#include "hal_io/hal_io.h"
#include "motor_control/time_util.h"

GateOutputs::GateOutputs(Io& io, const AdvancedSettings& adv) : io(io), adv(adv)
{
}

bool GateOutputs::isLocked() const
{
    return locked;
}

bool GateOutputs::isFlashOn() const
{
    return flash;
}

bool GateOutputs::isPassageLightOn() const
{
    return passageLight;
}

void GateOutputs::engageLock()
{
    // Solenoid: de-energize (Low) to lock. Magnet: energize (High) to lock.
    io.write(Output::Lock, !adv.getSolenoidMagnet());
    locked = true;

    io.write(Output::Flash, false);
    flash = false;
}

void GateOutputs::disengageLock(int preWarningMs)
{
    // Solenoid: energize (High) to unlock. Magnet: de-energize (Low) to unlock.
    io.write(Output::Lock, adv.getSolenoidMagnet());

    // Start the beacon (if not already on).
    if (!flash) {
        io.write(Output::Flash, true);
        flash = true;
    }

    // Pre-warning before the gate moves (caller decides if it is needed).
    sleepMs(preWarningMs);

    locked = false;
}

void GateOutputs::passageLightOn()
{
    io.write(Output::PassageLight, true);
    passageLight = true;
}

void GateOutputs::passageLightOff()
{
    io.write(Output::PassageLight, false);
    passageLight = false;
}
