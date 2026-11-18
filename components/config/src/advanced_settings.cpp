#include "config/advanced_settings.h"

#include "config/teach_settings.h"

AdvancedSettings::AdvancedSettings()
{
    applyFactoryDefaults(values.parameterSet, values);
}

// Copy one value under the mutex.
template <typename T>
T AdvancedSettings::read(const T& field) const
{
    mu.lock();
    const T copy = field;
    mu.unlock();
    return copy;
}

// Set one value under the mutex.
template <typename T>
void AdvancedSettings::write(T& field, T value)
{
    mu.lock();
    field = value;
    mu.unlock();
}

// Getters and setters: each holds the mutex only while copying one value.
ParameterSet AdvancedSettings::getParameterSet() const
{
    return read(values.parameterSet);
}

void AdvancedSettings::setParameterSet(ParameterSet set)
{
    write(values.parameterSet, set);
}

double AdvancedSettings::getPulsesPerMeter() const
{
    return read(values.pulsesPerMeter);
}

void AdvancedSettings::setPulsesPerMeter(double pulsesPerMeter)
{
    write(values.pulsesPerMeter, pulsesPerMeter);
}

float AdvancedSettings::getEncPpr() const
{
    return read(values.encPpr);
}

void AdvancedSettings::setEncPpr(float encPpr)
{
    write(values.encPpr, encPpr);
}

bool AdvancedSettings::getUseQuadratureEncoder() const
{
    return read(values.useQuadratureEncoder);
}

void AdvancedSettings::setUseQuadratureEncoder(bool use)
{
    write(values.useQuadratureEncoder, use);
}

bool AdvancedSettings::getInvertEncoderCount() const
{
    return read(values.invertEncoderCount);
}

void AdvancedSettings::setInvertEncoderCount(bool invert)
{
    write(values.invertEncoderCount, invert);
}

bool AdvancedSettings::getInvertMotorDirection() const
{
    return read(values.invertMotorDirection);
}

void AdvancedSettings::setInvertMotorDirection(bool invert)
{
    write(values.invertMotorDirection, invert);
}

bool AdvancedSettings::getUseDualDirectionInputs() const
{
    return read(values.useDualDirectionInputs);
}

void AdvancedSettings::setUseDualDirectionInputs(bool use)
{
    write(values.useDualDirectionInputs, use);
}

bool AdvancedSettings::getUseMotorDisable() const
{
    return read(values.useMotorDisable);
}

void AdvancedSettings::setUseMotorDisable(bool use)
{
    write(values.useMotorDisable, use);
}

double AdvancedSettings::getSpdTch() const
{
    return read(values.spdTch);
}

void AdvancedSettings::setSpdTch(double speed)
{
    write(values.spdTch, speed);
}

double AdvancedSettings::getSpdExtForce() const
{
    return read(values.spdExtForce);
}

void AdvancedSettings::setSpdExtForce(double speed)
{
    write(values.spdExtForce, speed);
}

double AdvancedSettings::getSpdUnlck() const
{
    return read(values.spdUnlck);
}

void AdvancedSettings::setSpdUnlck(double speed)
{
    write(values.spdUnlck, speed);
}

double AdvancedSettings::getSpdHiC() const
{
    return read(values.spdHiC);
}

void AdvancedSettings::setSpdHiC(double speed)
{
    write(values.spdHiC, speed);
}

double AdvancedSettings::getSpdLoC() const
{
    return read(values.spdLoC);
}

void AdvancedSettings::setSpdLoC(double speed)
{
    write(values.spdLoC, speed);
}

double AdvancedSettings::getSpdHiO() const
{
    return read(values.spdHiO);
}

void AdvancedSettings::setSpdHiO(double speed)
{
    write(values.spdHiO, speed);
}

double AdvancedSettings::getSpdLoO() const
{
    return read(values.spdLoO);
}

void AdvancedSettings::setSpdLoO(double speed)
{
    write(values.spdLoO, speed);
}

double AdvancedSettings::getAccC() const
{
    return read(values.accC);
}

void AdvancedSettings::setAccC(double acc)
{
    write(values.accC, acc);
}

double AdvancedSettings::getDecC() const
{
    return read(values.decC);
}

void AdvancedSettings::setDecC(double dec)
{
    write(values.decC, dec);
}

double AdvancedSettings::getAccO() const
{
    return read(values.accO);
}

void AdvancedSettings::setAccO(double acc)
{
    write(values.accO, acc);
}

double AdvancedSettings::getDecO() const
{
    return read(values.decO);
}

void AdvancedSettings::setDecO(double dec)
{
    write(values.decO, dec);
}

double AdvancedSettings::getAppC() const
{
    return read(values.appC);
}

void AdvancedSettings::setAppC(double meters)
{
    write(values.appC, meters);
}

double AdvancedSettings::getAppO() const
{
    return read(values.appO);
}

void AdvancedSettings::setAppO(double meters)
{
    write(values.appO, meters);
}

double AdvancedSettings::getTqOps() const
{
    return read(values.tqOps);
}

void AdvancedSettings::setTqOps(double torque)
{
    write(values.tqOps, torque);
}

double AdvancedSettings::getTqSfty() const
{
    return read(values.tqSfty);
}

void AdvancedSettings::setTqSfty(double torque)
{
    write(values.tqSfty, torque);
}

double AdvancedSettings::getTqSftyZoneStrt() const
{
    return read(values.tqSftyZoneStrt);
}

void AdvancedSettings::setTqSftyZoneStrt(double meters)
{
    write(values.tqSftyZoneStrt, meters);
}

double AdvancedSettings::getTqSftyZoneStop() const
{
    return read(values.tqSftyZoneStop);
}

void AdvancedSettings::setTqSftyZoneStop(double meters)
{
    write(values.tqSftyZoneStop, meters);
}

int AdvancedSettings::getTorqueRetryMaxCount() const
{
    return read(values.torqueRetryMaxCount);
}

void AdvancedSettings::setTorqueRetryMaxCount(int count)
{
    write(values.torqueRetryMaxCount, count);
}

bool AdvancedSettings::getBtAftTqLmt() const
{
    return read(values.btAftTqLmt);
}

void AdvancedSettings::setBtAftTqLmt(bool backtrack)
{
    write(values.btAftTqLmt, backtrack);
}

bool AdvancedSettings::getBtAftSe() const
{
    return read(values.btAftSe);
}

void AdvancedSettings::setBtAftSe(bool backtrack)
{
    write(values.btAftSe, backtrack);
}

int AdvancedSettings::getBtTime() const
{
    return read(values.btTime);
}

void AdvancedSettings::setBtTime(int ms)
{
    write(values.btTime, ms);
}

int AdvancedSettings::getSafetyEdgeWaitTime() const
{
    return read(values.safetyEdgeWaitTime);
}

void AdvancedSettings::setSafetyEdgeWaitTime(int seconds)
{
    write(values.safetyEdgeWaitTime, seconds);
}

bool AdvancedSettings::getSafetyResume() const
{
    return read(values.safetyResume);
}

void AdvancedSettings::setSafetyResume(bool resume)
{
    write(values.safetyResume, resume);
}

bool AdvancedSettings::getWaitSocDo() const
{
    return read(values.waitSocDo);
}

void AdvancedSettings::setWaitSocDo(bool wait)
{
    write(values.waitSocDo, wait);
}

bool AdvancedSettings::getWaitSoDo() const
{
    return read(values.waitSoDo);
}

void AdvancedSettings::setWaitSoDo(bool wait)
{
    write(values.waitSoDo, wait);
}

bool AdvancedSettings::getWaitSocDc() const
{
    return read(values.waitSocDc);
}

void AdvancedSettings::setWaitSocDc(bool wait)
{
    write(values.waitSocDc, wait);
}

bool AdvancedSettings::getWaitScDc() const
{
    return read(values.waitScDc);
}

void AdvancedSettings::setWaitScDc(bool wait)
{
    write(values.waitScDc, wait);
}

bool AdvancedSettings::getSolenoidMagnet() const
{
    return read(values.solenoidMagnet);
}

void AdvancedSettings::setSolenoidMagnet(bool magnet)
{
    write(values.solenoidMagnet, magnet);
}

int AdvancedSettings::getGateIdleLockTimeoutSeconds() const
{
    return read(values.gateIdleLockTimeoutSeconds);
}

void AdvancedSettings::setGateIdleLockTimeoutSeconds(int seconds)
{
    write(values.gateIdleLockTimeoutSeconds, seconds);
}

int AdvancedSettings::getUnlckMoveTime() const
{
    return read(values.unlckMoveTime);
}

void AdvancedSettings::setUnlckMoveTime(int ms)
{
    write(values.unlckMoveTime, ms);
}

int AdvancedSettings::getUnlckTime() const
{
    return read(values.unlckTime);
}

void AdvancedSettings::setUnlckTime(int ms)
{
    write(values.unlckTime, ms);
}

int AdvancedSettings::getOpsLimit() const
{
    return read(values.opsLimit);
}

void AdvancedSettings::setOpsLimit(int seconds)
{
    write(values.opsLimit, seconds);
}

bool AdvancedSettings::getSimOpenEnabled() const
{
    return read(values.simOpenEnabled);
}

void AdvancedSettings::setSimOpenEnabled(bool enabled)
{
    write(values.simOpenEnabled, enabled);
}

int AdvancedSettings::getSimOpenDelayS() const
{
    return read(values.simOpenDelayS);
}

void AdvancedSettings::setSimOpenDelayS(int seconds)
{
    write(values.simOpenDelayS, seconds);
}

uint32_t AdvancedSettings::getStopCount() const
{
    return read(stopCount);
}

void AdvancedSettings::setStopCount(uint32_t count)
{
    write(stopCount, count);
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
