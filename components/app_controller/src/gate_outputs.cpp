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
