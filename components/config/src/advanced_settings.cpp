#include "config/advanced_settings.h"

#include "config/teach_settings.h"

AdvancedSettings::AdvancedSettings()
{
    applyFactoryDefaults(values.parameterSet, values);
}

void AdvancedSettings::applyDefaults(ParameterSet set)
{
    mu.lock();
    applyFactoryDefaults(set, values);
    mu.unlock();
}

void AdvancedSettings::resetToDefaults()
{
    AdvancedValues fresh;
    applyFactoryDefaults(fresh.parameterSet, fresh);
    mu.lock();
    values = fresh;
    mu.unlock();
}

AdvancedValues AdvancedSettings::getValues() const
{
    mu.lock();
    const AdvancedValues copy = values;
    mu.unlock();
    return copy;
}

void AdvancedSettings::setValues(const AdvancedValues& values)
{
    mu.lock();
    this->values = values;
    mu.unlock();
}

int AdvancedSettings::getAppCPulses() const
{
    mu.lock();
    const int pulses = static_cast<int>(values.appC * values.pulsesPerMeter);
    mu.unlock();
    return pulses;
}

int AdvancedSettings::getAppOPulses() const
{
    mu.lock();
    const int pulses = static_cast<int>(values.appO * values.pulsesPerMeter);
    mu.unlock();
    return pulses;
}

int AdvancedSettings::getTqSftyZoneStrtPulses() const
{
    mu.lock();
    const int pulses = static_cast<int>(values.tqSftyZoneStrt * values.pulsesPerMeter);
    mu.unlock();
    return pulses;
}

double AdvancedSettings::getTqSftyZoneStopPulses() const
{
    mu.lock();
    const int pulses = static_cast<int>(values.tqSftyZoneStop * values.pulsesPerMeter);
    mu.unlock();
    return pulses;
}

double AdvancedSettings::getPedesPosMarginPulses() const
{
    mu.lock();
    const int pulses = static_cast<int>(kPedesPosMargin * values.pulsesPerMeter);
    mu.unlock();
    return pulses;
}

void AdvancedSettings::increaseStopCount()
{
    mu.lock();
    stopCount = stopCount + 1;
    mu.unlock();
}

double AdvancedSettings::computeGatePassageMeters(const TeachSettings& teach) const
{
    const double pulsesPerMeter = getPulsesPerMeter();
    if (pulsesPerMeter == 0.0) {
        return 0.0;
    }
    // POS_O is in pulses; convert to meters
    return static_cast<double>(teach.getPosO()) / pulsesPerMeter;
}
