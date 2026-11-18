#ifndef CONFIG_ADVANCED_SETTINGS_H
#define CONFIG_ADVANCED_SETTINGS_H

#include <cstdint>
#include <mutex>

#include "config/factory_defaults.h"

class TeachSettings;

/**
 * @brief All advanced settings as plain values; saved as one NVS blob (was the AdvancedSettings fields).
 */
struct AdvancedValues {
    // Gate model
    ParameterSet parameterSet = ParameterSet::Terminus;  ///< was PARAMETER_SET

    // Encoder and travel
    double pulsesPerMeter = 457.8;        ///< encoder pulses per meter of gate travel (was PULSES_PER_METER)
    float encPpr = 0.0f;                  ///< encoder pulses per motor rotation (was ENC_PPR)
    bool useQuadratureEncoder = false;    ///< was USE_QUADRATURE_ENCODER
    bool invertEncoderCount = false;      ///< was INVERT_ENCODER_COUNT

    // Motor driver
    bool invertMotorDirection = false;    ///< was INVERT_MOTOR_DIRECTION
    bool useDualDirectionInputs = false;  ///< run-open / run-close pins instead of run + direction (was USE_DUAL_DIRECTION_INPUTS)
    bool useMotorDisable = false;         ///< drive the motor disable pin (was USE_MOTOR_DISABLE)

    // Speeds, 0..1 of full speed
    double spdTch = 0.0;       ///< teach run (was SPD_TCH)
    double spdExtForce = 0.0;  ///< extra push when closed but the close limit is not reached (was SPD_EXT_FORCE)
    double spdUnlck = 0.0;     ///< unlock move (was SPD_UNLCK)
    double spdHiC = 0.0;       ///< closing, high speed (was SPD_HI_C)
    double spdLoC = 0.0;       ///< closing, slow speed near the end (was SPD_LO_C)
    double spdHiO = 0.0;       ///< opening, high speed (was SPD_HI_O)
    double spdLoO = 0.0;       ///< opening, slow speed near the end (was SPD_LO_O)

    // Ramps
    double accC = 0.0;  ///< closing ramp up (was ACC_C)
    double decC = 0.0;  ///< closing ramp down (was DEC_C)
    double accO = 0.0;  ///< opening ramp up (was ACC_O)
    double decO = 0.0;  ///< opening ramp down (was DEC_O)

    // Slow-down distances, meters before the end position
    double appC = 0.0;  ///< before closed (was APP_C)
    double appO = 0.0;  ///< before open (was APP_O)

    // Torque, 0..1
    double tqOps = 0.0;            ///< normal operation (was TQ_OPS)
    double tqSfty = 0.0;           ///< inside the safety zone near closed (was TQ_SFTY)
    double tqSftyZoneStrt = 0.9;   ///< safety zone start, meters from closed (was TQ_STFY_ZONE_STRT)
    double tqSftyZoneStop = 0.1;   ///< safety zone end, meters from closed (was TQ_STFY_ZONE_STOP)
    int torqueRetryMaxCount = 4;   ///< torque trips before the gate gives up (was TorqueRetryMaxCount)

    // Safety reactions
    bool btAftTqLmt = true;        ///< back off after a torque limit (was BT_AFT_TQ_LMT)
    bool btAftSe = false;          ///< back off after the safety edge (was BT_AFT_SE)
    int btTime = 1000;             ///< back-off time in ms (was BT_TIME)
    int safetyEdgeWaitTime = 3;    ///< s the safety must stay OK before it is released (was SafetyEdgeWaitTime)
    bool safetyResume = true;      ///< true: continue after safety OK; false: stop all movement (was SafetyResume)
    bool waitSocDo = true;         ///< pressure sensor while opening: true = continue, false = reverse after OK (was WAIT_SOC_DO)
    bool waitSoDo = false;         ///< photocell (swing gate) while opening: same choice (was WAIT_SO_DO)
    bool waitSocDc = true;         ///< pressure sensor while closing: same choice (was WAIT_SOC_DC)
    bool waitScDc = false;         ///< torque or photocell while closing: true = continue closing, false = reverse (was WAIT_SC_DC)

    // Lock
    bool solenoidMagnet = false;          ///< the lock is a solenoid magnet (was SOLENOID_MAGNET)
    int gateIdleLockTimeoutSeconds = 20;  ///< was GateIdleLockTimeoutSeconds
    int unlckMoveTime = 300;              ///< ms (was UNLCK_MOVE_TIME)
    int unlckTime = 1000;                 ///< ms (was UNLCK_TIME)

    // Limits and duration test
    int opsLimit = 180;            ///< max run time in s (was OpsLimt)
    bool simOpenEnabled = false;   ///< duration test: open again after fully closed (was SIM_OPEN_ENABLED)
    int simOpenDelayS = 300;       ///< s after fully closed, 0 = at once (was SIM_OPEN_DELAY_S)
};

/**
 * @brief Advanced (installer) settings: speeds, ramps, torques, safety and the gate model (was AdvancedSettings).
 * Thread safe: every getter and setter takes the mutex for one copy.
 */
class AdvancedSettings {
public:
    /**
     * @brief Start values plus the defaults of the start model, Terminus (was the constructor).
     */
    AdvancedSettings();

    /**
     * @brief Load the factory defaults of a gate model; other settings stay (was ApplyDefaults).
     */
    void applyDefaults(ParameterSet set);

    /**
     * @brief Every setting back to its start value and the Terminus defaults; the stop count stays.
     * (was new AdvancedSettings() in the web reset and in LoadConfigs)
     */
    void resetToDefaults();

    /**
     * @brief Copy of all settings, taken under one lock (new).
     */
    AdvancedValues getValues() const;

    /**
     * @brief Replace all settings in one go; the stop count stays (new).
     */
    void setValues(const AdvancedValues& values);

    // Gate model
    /**
     * @brief The gate model (was GetParameterSet / PARAMETER_SET).
     */
    ParameterSet getParameterSet() const { return read(values.parameterSet); }
    /**
     * @brief Set the gate model only; call applyDefaults() to load its defaults (was PARAMETER_SET).
     */
    void setParameterSet(ParameterSet set) { write(values.parameterSet, set); }

    // Encoder and travel
    /**
     * @brief Encoder pulses per meter of gate travel (was PULSES_PER_METER).
     */
    double getPulsesPerMeter() const { return read(values.pulsesPerMeter); }
    /**
     * @brief Set PULSES_PER_METER.
     */
    void setPulsesPerMeter(double pulsesPerMeter) { write(values.pulsesPerMeter, pulsesPerMeter); }
    /**
     * @brief Encoder pulses per motor rotation (was ENC_PPR).
     */
    float getEncPpr() const { return read(values.encPpr); }
    /**
     * @brief Set ENC_PPR.
     */
    void setEncPpr(float encPpr) { write(values.encPpr, encPpr); }
    /**
     * @brief Read the encoder as quadrature A/B (was USE_QUADRATURE_ENCODER).
     */
    bool getUseQuadratureEncoder() const { return read(values.useQuadratureEncoder); }
    /**
     * @brief Set USE_QUADRATURE_ENCODER; takes effect after a reboot.
     */
    void setUseQuadratureEncoder(bool use) { write(values.useQuadratureEncoder, use); }
    /**
     * @brief Count the encoder the other way round (was INVERT_ENCODER_COUNT).
     */
    bool getInvertEncoderCount() const { return read(values.invertEncoderCount); }
    /**
     * @brief Set INVERT_ENCODER_COUNT.
     */
    void setInvertEncoderCount(bool invert) { write(values.invertEncoderCount, invert); }

    // Motor driver
    /**
     * @brief Swap the motor's open and close direction (was INVERT_MOTOR_DIRECTION).
     */
    bool getInvertMotorDirection() const { return read(values.invertMotorDirection); }
    /**
     * @brief Set INVERT_MOTOR_DIRECTION.
     */
    void setInvertMotorDirection(bool invert) { write(values.invertMotorDirection, invert); }
    /**
     * @brief Drive the motor with run-open / run-close pins (was USE_DUAL_DIRECTION_INPUTS).
     */
    bool getUseDualDirectionInputs() const { return read(values.useDualDirectionInputs); }
    /**
     * @brief Set USE_DUAL_DIRECTION_INPUTS.
     */
    void setUseDualDirectionInputs(bool use) { write(values.useDualDirectionInputs, use); }
    /**
     * @brief Use the motor disable pin (was USE_MOTOR_DISABLE).
     */
    bool getUseMotorDisable() const { return read(values.useMotorDisable); }
    /**
     * @brief Set USE_MOTOR_DISABLE.
     */
    void setUseMotorDisable(bool use) { write(values.useMotorDisable, use); }

    // Speeds
    /**
     * @brief Teach run speed, 0..1 (was SPD_TCH).
     */
    double getSpdTch() const { return read(values.spdTch); }
    /**
     * @brief Set SPD_TCH.
     */
    void setSpdTch(double speed) { write(values.spdTch, speed); }
    /**
     * @brief Extra-force speed when closed without the close limit, 0..1 (was SPD_EXT_FORCE).
     */
    double getSpdExtForce() const { return read(values.spdExtForce); }
    /**
     * @brief Set SPD_EXT_FORCE.
     */
    void setSpdExtForce(double speed) { write(values.spdExtForce, speed); }
    /**
     * @brief Unlock move speed, 0..1 (was SPD_UNLCK).
     */
    double getSpdUnlck() const { return read(values.spdUnlck); }
    /**
     * @brief Set SPD_UNLCK.
     */
    void setSpdUnlck(double speed) { write(values.spdUnlck, speed); }
    /**
     * @brief Closing high speed, 0..1 (was SPD_HI_C).
     */
    double getSpdHiC() const { return read(values.spdHiC); }
    /**
     * @brief Set SPD_HI_C.
     */
    void setSpdHiC(double speed) { write(values.spdHiC, speed); }
    /**
     * @brief Closing slow speed, 0..1 (was SPD_LO_C).
     */
    double getSpdLoC() const { return read(values.spdLoC); }
    /**
     * @brief Set SPD_LO_C.
     */
    void setSpdLoC(double speed) { write(values.spdLoC, speed); }
    /**
     * @brief Opening high speed, 0..1 (was SPD_HI_O).
     */
    double getSpdHiO() const { return read(values.spdHiO); }
    /**
     * @brief Set SPD_HI_O.
     */
    void setSpdHiO(double speed) { write(values.spdHiO, speed); }
    /**
     * @brief Opening slow speed, 0..1 (was SPD_LO_O).
     */
    double getSpdLoO() const { return read(values.spdLoO); }
    /**
     * @brief Set SPD_LO_O.
     */
    void setSpdLoO(double speed) { write(values.spdLoO, speed); }

    // Ramps
    /**
     * @brief Closing ramp up (was ACC_C).
     */
    double getAccC() const { return read(values.accC); }
    /**
     * @brief Set ACC_C.
     */
    void setAccC(double acc) { write(values.accC, acc); }
    /**
     * @brief Closing ramp down (was DEC_C).
     */
    double getDecC() const { return read(values.decC); }
    /**
     * @brief Set DEC_C.
     */
    void setDecC(double dec) { write(values.decC, dec); }
    /**
     * @brief Opening ramp up (was ACC_O).
     */
    double getAccO() const { return read(values.accO); }
    /**
     * @brief Set ACC_O.
     */
    void setAccO(double acc) { write(values.accO, acc); }
    /**
     * @brief Opening ramp down (was DEC_O).
     */
    double getDecO() const { return read(values.decO); }
    /**
     * @brief Set DEC_O.
     */
    void setDecO(double dec) { write(values.decO, dec); }

    // Slow-down distances
    /**
     * @brief Slow-down distance before closed, meters (was APP_C).
     */
    double getAppC() const { return read(values.appC); }
    /**
     * @brief Set APP_C.
     */
    void setAppC(double meters) { write(values.appC, meters); }
    /**
     * @brief Slow-down distance before open, meters (was APP_O).
     */
    double getAppO() const { return read(values.appO); }
    /**
     * @brief Set APP_O.
     */
    void setAppO(double meters) { write(values.appO, meters); }
    /**
     * @brief APP_C in encoder pulses (was APP_C_Pulses).
     */
    int getAppCPulses() const;
    /**
     * @brief APP_O in encoder pulses (was APP_O_Pulses).
     */
    int getAppOPulses() const;

    // Torque
    /**
     * @brief Torque limit in normal operation, 0..1 (was TQ_OPS).
     */
    double getTqOps() const { return read(values.tqOps); }
    /**
     * @brief Set TQ_OPS.
     */
    void setTqOps(double torque) { write(values.tqOps, torque); }
    /**
     * @brief Torque limit inside the safety zone, 0..1 (was TQ_SFTY).
     */
    double getTqSfty() const { return read(values.tqSfty); }
    /**
     * @brief Set TQ_SFTY.
     */
    void setTqSfty(double torque) { write(values.tqSfty, torque); }
    /**
     * @brief Safety zone start, meters from closed (was TQ_STFY_ZONE_STRT).
     */
    double getTqSftyZoneStrt() const { return read(values.tqSftyZoneStrt); }
    /**
     * @brief Set TQ_STFY_ZONE_STRT.
     */
    void setTqSftyZoneStrt(double meters) { write(values.tqSftyZoneStrt, meters); }
    /**
     * @brief Safety zone end, meters from closed (was TQ_STFY_ZONE_STOP).
     */
    double getTqSftyZoneStop() const { return read(values.tqSftyZoneStop); }
    /**
     * @brief Set TQ_STFY_ZONE_STOP.
     */
    void setTqSftyZoneStop(double meters) { write(values.tqSftyZoneStop, meters); }
    /**
     * @brief Safety zone start in pulses (was TQ_STFY_ZONE_STRT_PULSES).
     */
    int getTqSftyZoneStrtPulses() const;
    /**
     * @brief Safety zone end in pulses, whole pulses as a double (was TQ_STFY_ZONE_STOP_PULSES).
     */
    double getTqSftyZoneStopPulses() const;
    /**
     * @brief Torque trips allowed before the gate gives up (was TorqueRetryMaxCount).
     */
    int getTorqueRetryMaxCount() const { return read(values.torqueRetryMaxCount); }
    /**
     * @brief Set TorqueRetryMaxCount.
     */
    void setTorqueRetryMaxCount(int count) { write(values.torqueRetryMaxCount, count); }

    // Safety reactions
    /**
     * @brief Back off after a torque limit (was BT_AFT_TQ_LMT).
     */
    bool getBtAftTqLmt() const { return read(values.btAftTqLmt); }
    /**
     * @brief Set BT_AFT_TQ_LMT.
     */
    void setBtAftTqLmt(bool backtrack) { write(values.btAftTqLmt, backtrack); }
    /**
     * @brief Back off after the safety edge (was BT_AFT_SE).
     */
    bool getBtAftSe() const { return read(values.btAftSe); }
    /**
     * @brief Set BT_AFT_SE.
     */
    void setBtAftSe(bool backtrack) { write(values.btAftSe, backtrack); }
    /**
     * @brief Back-off time in ms (was BT_TIME).
     */
    int getBtTime() const { return read(values.btTime); }
    /**
     * @brief Set BT_TIME.
     */
    void setBtTime(int ms) { write(values.btTime, ms); }
    /**
     * @brief Seconds the safety must stay OK before it is released (was SafetyEdgeWaitTime).
     */
    int getSafetyEdgeWaitTime() const { return read(values.safetyEdgeWaitTime); }
    /**
     * @brief Set SafetyEdgeWaitTime.
     */
    void setSafetyEdgeWaitTime(int seconds) { write(values.safetyEdgeWaitTime, seconds); }
    /**
     * @brief true: continue the movement after safety OK; false: stop (was SafetyResume).
     */
    bool getSafetyResume() const { return read(values.safetyResume); }
    /**
     * @brief Set SafetyResume.
     */
    void setSafetyResume(bool resume) { write(values.safetyResume, resume); }
    /**
     * @brief Pressure sensor while opening: true = continue, false = reverse (was WAIT_SOC_DO).
     */
    bool getWaitSocDo() const { return read(values.waitSocDo); }
    /**
     * @brief Set WAIT_SOC_DO.
     */
    void setWaitSocDo(bool wait) { write(values.waitSocDo, wait); }
    /**
     * @brief Photocell (swing gate) while opening: true = continue, false = reverse (was WAIT_SO_DO).
     */
    bool getWaitSoDo() const { return read(values.waitSoDo); }
    /**
     * @brief Set WAIT_SO_DO.
     */
    void setWaitSoDo(bool wait) { write(values.waitSoDo, wait); }
    /**
     * @brief Pressure sensor while closing: true = continue, false = reverse (was WAIT_SOC_DC).
     */
    bool getWaitSocDc() const { return read(values.waitSocDc); }
    /**
     * @brief Set WAIT_SOC_DC.
     */
    void setWaitSocDc(bool wait) { write(values.waitSocDc, wait); }
    /**
     * @brief Torque or photocell while closing: true = continue, false = reverse (was WAIT_SC_DC).
     */
    bool getWaitScDc() const { return read(values.waitScDc); }
    /**
     * @brief Set WAIT_SC_DC.
     */
    void setWaitScDc(bool wait) { write(values.waitScDc, wait); }

    // Pedestrian
    /**
     * @brief Tolerance around the pedestrian position, whole pulses as a double (was PEDES_POS_MARGIN_PULSES).
     */
    double getPedesPosMarginPulses() const;

    // Lock
    /**
     * @brief The lock is a solenoid magnet (was SOLENOID_MAGNET).
     */
    bool getSolenoidMagnet() const { return read(values.solenoidMagnet); }
    /**
     * @brief Set SOLENOID_MAGNET.
     */
    void setSolenoidMagnet(bool magnet) { write(values.solenoidMagnet, magnet); }
    /**
     * @brief Idle seconds after which the next start gets the lock pre-warning (was GateIdleLockTimeoutSeconds).
     */
    int getGateIdleLockTimeoutSeconds() const { return read(values.gateIdleLockTimeoutSeconds); }
    /**
     * @brief Set GateIdleLockTimeoutSeconds.
     */
    void setGateIdleLockTimeoutSeconds(int seconds) { write(values.gateIdleLockTimeoutSeconds, seconds); }
    /**
     * @brief Unlock move time in ms (was UNLCK_MOVE_TIME).
     */
    int getUnlckMoveTime() const { return read(values.unlckMoveTime); }
    /**
     * @brief Set UNLCK_MOVE_TIME.
     */
    void setUnlckMoveTime(int ms) { write(values.unlckMoveTime, ms); }
    /**
     * @brief Unlock time in ms (was UNLCK_TIME; the gate does not use it).
     */
    int getUnlckTime() const { return read(values.unlckTime); }
    /**
     * @brief Set UNLCK_TIME.
     */
    void setUnlckTime(int ms) { write(values.unlckTime, ms); }

    // Limits and duration test
    /**
     * @brief Max run time in seconds (was OpsLimt).
     */
    int getOpsLimit() const { return read(values.opsLimit); }
    /**
     * @brief Set OpsLimt.
     */
    void setOpsLimit(int seconds) { write(values.opsLimit, seconds); }
    /**
     * @brief Duration test: open again after fully closed (was SIM_OPEN_ENABLED).
     */
    bool getSimOpenEnabled() const { return read(values.simOpenEnabled); }
    /**
     * @brief Set SIM_OPEN_ENABLED.
     */
    void setSimOpenEnabled(bool enabled) { write(values.simOpenEnabled, enabled); }
    /**
     * @brief Duration test delay in seconds, 0 = at once (was SIM_OPEN_DELAY_S).
     */
    int getSimOpenDelayS() const { return read(values.simOpenDelayS); }
    /**
     * @brief Set SIM_OPEN_DELAY_S.
     */
    void setSimOpenDelayS(int seconds) { write(values.simOpenDelayS, seconds); }

    // Stop counter (cycles)
    /**
     * @brief Number of completed close cycles (was StopCount).
     */
    uint32_t getStopCount() const { return read(stopCount); }
    /**
     * @brief Set StopCount (Modbus write, web "Reset Cycles"); RAM only until saved.
     */
    void setStopCount(uint32_t count) { write(stopCount, count); }
    /**
     * @brief Add one to the stop count (was IncreaseStopCount).
     */
    void increaseStopCount();

    /**
     * @brief Taught travel in meters: POS_O / PULSES_PER_METER, 0 when that is 0 (was ComputeGatePassageMeters).
     */
    double computeGatePassageMeters(const TeachSettings& teach) const;

private:
    static constexpr double kPedesPosMargin = 0.1;   ///< meters (was PEDES_POS_MARGIN)

    /**
     * @brief Copy one value under the mutex.
     */
    template <typename T>
    T read(const T& field) const
    {
        mu.lock();
        const T copy = field;
        mu.unlock();
        return copy;
    }

    /**
     * @brief Set one value under the mutex.
     */
    template <typename T>
    void write(T& field, T value)
    {
        mu.lock();
        field = value;
        mu.unlock();
    }

    mutable std::mutex mu;   ///< web/modbus write while the gate reads
    AdvancedValues values;   ///< all saved settings
    uint32_t stopCount = 0;  ///< was the static _StopCount
};

#endif // CONFIG_ADVANCED_SETTINGS_H
