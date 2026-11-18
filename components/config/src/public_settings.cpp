#include "config/public_settings.h"

// Copy one value under the mutex.
template <typename T>
T PublicSettings::read(const T& field) const
{
    mu.lock();
    const T copy = field;
    mu.unlock();
    return copy;
}

// Set one value under the mutex.
template <typename T>
void PublicSettings::write(T& field, const T& value)
{
    mu.lock();
    field = value;
    mu.unlock();
}

// Getters and setters: each holds the mutex only while copying one value.
PublicValues PublicSettings::getValues() const
{
    return read(values);
}

void PublicSettings::setValues(const PublicValues& values)
{
    write(this->values, values);
}

int PublicSettings::getAutoCloseDelay() const
{
    return read(values.autoCloseDelay);
}

void PublicSettings::setAutoCloseDelay(int seconds)
{
    write(values.autoCloseDelay, seconds);
}

float PublicSettings::getPedestrianModeDistance() const
{
    return read(values.pedestrianModeDistance);
}

void PublicSettings::setPedestrianModeDistance(float meters)
{
    write(values.pedestrianModeDistance, meters);
}

int PublicSettings::getPedestrianModeDistancePulse(double pulsesPerMeter) const
{
    return static_cast<int>(getPedestrianModeDistance() * pulsesPerMeter);
}

int PublicSettings::getPassageLightDelay() const
{
    return read(values.passageLightDelay);
}

void PublicSettings::setPassageLightDelay(int seconds)
{
    write(values.passageLightDelay, seconds);
}

int PublicSettings::getBeaconPreWarningTime() const
{
    return read(values.beaconPreWarningTime);
}

void PublicSettings::setBeaconPreWarningTime(int seconds)
{
    write(values.beaconPreWarningTime, seconds);
}

int PublicSettings::getModbusAddress() const
{
    return read(values.modbusAddress);
}

void PublicSettings::setModbusAddress(int address)
{
    write(values.modbusAddress, address);
}

bool PublicSettings::getAutoRedirectEnabled() const
{
    return read(values.autoRedirectEnabled);
}

void PublicSettings::setAutoRedirectEnabled(bool enabled)
{
    write(values.autoRedirectEnabled, enabled);
}
