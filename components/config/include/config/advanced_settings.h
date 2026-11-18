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
    ParameterSet getParameterSet() const;
    /**
     * @brief Set the gate model only; call applyDefaults() to load its defaults (was PARAMETER_SET).
     */
    void setParameterSet(ParameterSet set);

    // Encoder and travel
    /**
     * @brief Encoder pulses per meter of gate travel (was PULSES_PER_METER).
     */
    double getPulsesPerMeter() const;
    /**
     * @brief Set PULSES_PER_METER.
     */
    void setPulsesPerMeter(double pulsesPerMeter);
    /**
     * @brief Encoder pulses per motor rotation (was ENC_PPR).
     */
    float getEncPpr() const;
    /**
     * @brief Set ENC_PPR.
     */
    void setEncPpr(float encPpr);
    /**
     * @brief Read the encoder as quadrature A/B (was USE_QUADRATURE_ENCODER).
     */
    bool getUseQuadratureEncoder() const;
    /**
     * @brief Set USE_QUADRATURE_ENCODER; takes effect after a reboot.
     */
    void setUseQuadratureEncoder(bool use);
    /**
     * @brief Count the encoder the other way round (was INVERT_ENCODER_COUNT).
     */
    bool getInvertEncoderCount() const;
    /**
     * @brief Set INVERT_ENCODER_COUNT.
     */
    void setInvertEncoderCount(bool invert);

    // Motor driver
    /**
     * @brief Swap the motor's open and close direction (was INVERT_MOTOR_DIRECTION).
     */
    bool getInvertMotorDirection() const;
    /**
     * @brief Set INVERT_MOTOR_DIRECTION.
     */
    void setInvertMotorDirection(bool invert);
    /**
     * @brief Drive the motor with run-open / run-close pins (was USE_DUAL_DIRECTION_INPUTS).
     */
    bool getUseDualDirectionInputs() const;
    /**
     * @brief Set USE_DUAL_DIRECTION_INPUTS.
     */
    void setUseDualDirectionInputs(bool use);
    /**
     * @brief Use the motor disable pin (was USE_MOTOR_DISABLE).
     */
    bool getUseMotorDisable() const;
    /**
     * @brief Set USE_MOTOR_DISABLE.
     */
    void setUseMotorDisable(bool use);

    // Speeds
    /**
     * @brief Teach run speed, 0..1 (was SPD_TCH).
     */
    double getSpdTch() const;
    /**
     * @brief Set SPD_TCH.
     */
    void setSpdTch(double speed);
    /**
     * @brief Extra-force speed when closed without the close limit, 0..1 (was SPD_EXT_FORCE).
     */
    double getSpdExtForce() const;
    /**
     * @brief Set SPD_EXT_FORCE.
     */
    void setSpdExtForce(double speed);
    /**
     * @brief Unlock move speed, 0..1 (was SPD_UNLCK).
     */
    double getSpdUnlck() const;
    /**
     * @brief Set SPD_UNLCK.
     */
    void setSpdUnlck(double speed);
    /**
     * @brief Closing high speed, 0..1 (was SPD_HI_C).
     */
    double getSpdHiC() const;
    /**
     * @brief Set SPD_HI_C.
     */
    void setSpdHiC(double speed);
    /**
     * @brief Closing slow speed, 0..1 (was SPD_LO_C).
     */
    double getSpdLoC() const;
    /**
     * @brief Set SPD_LO_C.
     */
    void setSpdLoC(double speed);
    /**
     * @brief Opening high speed, 0..1 (was SPD_HI_O).
     */
    double getSpdHiO() const;
    /**
     * @brief Set SPD_HI_O.
     */
    void setSpdHiO(double speed);
    /**
     * @brief Opening slow speed, 0..1 (was SPD_LO_O).
     */
    double getSpdLoO() const;
    /**
     * @brief Set SPD_LO_O.
     */
    void setSpdLoO(double speed);

    // Ramps
    /**
     * @brief Closing ramp up (was ACC_C).
     */
    double getAccC() const;
    /**
     * @brief Set ACC_C.
     */
    void setAccC(double acc);
    /**
     * @brief Closing ramp down (was DEC_C).
     */
    double getDecC() const;
    /**
     * @brief Set DEC_C.
     */
    void setDecC(double dec);
    /**
     * @brief Opening ramp up (was ACC_O).
     */
    double getAccO() const;
    /**
     * @brief Set ACC_O.
     */
    void setAccO(double acc);
    /**
     * @brief Opening ramp down (was DEC_O).
     */
    double getDecO() const;
    /**
     * @brief Set DEC_O.
     */
    void setDecO(double dec);

    // Slow-down distances
    /**
     * @brief Slow-down distance before closed, meters (was APP_C).
     */
    double getAppC() const;
    /**
     * @brief Set APP_C.
     */
    void setAppC(double meters);
    /**
     * @brief Slow-down distance before open, meters (was APP_O).
     */
    double getAppO() const;
    /**
     * @brief Set APP_O.
     */
    void setAppO(double meters);
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
    double getTqOps() const;
    /**
     * @brief Set TQ_OPS.
     */
    void setTqOps(double torque);
    /**
     * @brief Torque limit inside the safety zone, 0..1 (was TQ_SFTY).
     */
    double getTqSfty() const;
    /**
     * @brief Set TQ_SFTY.
     */
    void setTqSfty(double torque);
    /**
     * @brief Safety zone start, meters from closed (was TQ_STFY_ZONE_STRT).
     */
    double getTqSftyZoneStrt() const;
    /**
     * @brief Set TQ_STFY_ZONE_STRT.
     */
    void setTqSftyZoneStrt(double meters);
    /**
     * @brief Safety zone end, meters from closed (was TQ_STFY_ZONE_STOP).
     */
    double getTqSftyZoneStop() const;
    /**
     * @brief Set TQ_STFY_ZONE_STOP.
     */
    void setTqSftyZoneStop(double meters);
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
    int getTorqueRetryMaxCount() const;
    /**
     * @brief Set TorqueRetryMaxCount.
     */
    void setTorqueRetryMaxCount(int count);

    // Safety reactions
    /**
     * @brief Back off after a torque limit (was BT_AFT_TQ_LMT).
     */
    bool getBtAftTqLmt() const;
    /**
     * @brief Set BT_AFT_TQ_LMT.
     */
    void setBtAftTqLmt(bool backtrack);
    /**
     * @brief Back off after the safety edge (was BT_AFT_SE).
     */
    bool getBtAftSe() const;
    /**
     * @brief Set BT_AFT_SE.
     */
    void setBtAftSe(bool backtrack);
    /**
     * @brief Back-off time in ms (was BT_TIME).
     */
    int getBtTime() const;
    /**
     * @brief Set BT_TIME.
     */
    void setBtTime(int ms);
    /**
     * @brief Seconds the safety must stay OK before it is released (was SafetyEdgeWaitTime).
     */
    int getSafetyEdgeWaitTime() const;
    /**
     * @brief Set SafetyEdgeWaitTime.
     */
    void setSafetyEdgeWaitTime(int seconds);
    /**
     * @brief true: continue the movement after safety OK; false: stop (was SafetyResume).
     */
    bool getSafetyResume() const;
    /**
     * @brief Set SafetyResume.
     */
    void setSafetyResume(bool resume);
    /**
     * @brief Pressure sensor while opening: true = continue, false = reverse (was WAIT_SOC_DO).
     */
    bool getWaitSocDo() const;
    /**
     * @brief Set WAIT_SOC_DO.
     */
    void setWaitSocDo(bool wait);
    /**
     * @brief Photocell (swing gate) while opening: true = continue, false = reverse (was WAIT_SO_DO).
     */
    bool getWaitSoDo() const;
    /**
     * @brief Set WAIT_SO_DO.
     */
    void setWaitSoDo(bool wait);
    /**
     * @brief Pressure sensor while closing: true = continue, false = reverse (was WAIT_SOC_DC).
     */
    bool getWaitSocDc() const;
    /**
     * @brief Set WAIT_SOC_DC.
     */
    void setWaitSocDc(bool wait);
    /**
     * @brief Torque or photocell while closing: true = continue, false = reverse (was WAIT_SC_DC).
     */
    bool getWaitScDc() const;
    /**
     * @brief Set WAIT_SC_DC.
     */
    void setWaitScDc(bool wait);

    // Pedestrian
    /**
     * @brief Tolerance around the pedestrian position, whole pulses as a double (was PEDES_POS_MARGIN_PULSES).
     */
    double getPedesPosMarginPulses() const;

    // Lock
    /**
     * @brief The lock is a solenoid magnet (was SOLENOID_MAGNET).
     */
    bool getSolenoidMagnet() const;
    /**
     * @brief Set SOLENOID_MAGNET.
     */
    void setSolenoidMagnet(bool magnet);
    /**
     * @brief Idle seconds after which the next start gets the lock pre-warning (was GateIdleLockTimeoutSeconds).
     */
    int getGateIdleLockTimeoutSeconds() const;
    /**
     * @brief Set GateIdleLockTimeoutSeconds.
     */
    void setGateIdleLockTimeoutSeconds(int seconds);
    /**
     * @brief Unlock move time in ms (was UNLCK_MOVE_TIME).
     */
    int getUnlckMoveTime() const;
    /**
     * @brief Set UNLCK_MOVE_TIME.
     */
    void setUnlckMoveTime(int ms);
    /**
     * @brief Unlock time in ms (was UNLCK_TIME; the gate does not use it).
     */
    int getUnlckTime() const;
    /**
     * @brief Set UNLCK_TIME.
     */
    void setUnlckTime(int ms);

    // Limits and duration test
    /**
     * @brief Max run time in seconds (was OpsLimt).
     */
    int getOpsLimit() const;
    /**
     * @brief Set OpsLimt.
     */
    void setOpsLimit(int seconds);
    /**
     * @brief Duration test: open again after fully closed (was SIM_OPEN_ENABLED).
     */
    bool getSimOpenEnabled() const;
    /**
     * @brief Set SIM_OPEN_ENABLED.
     */
    void setSimOpenEnabled(bool enabled);
    /**
     * @brief Duration test delay in seconds, 0 = at once (was SIM_OPEN_DELAY_S).
     */
    int getSimOpenDelayS() const;
    /**
     * @brief Set SIM_OPEN_DELAY_S.
     */
    void setSimOpenDelayS(int seconds);

    // Stop counter (cycles)
    /**
     * @brief Number of completed close cycles (was StopCount).
     */
    uint32_t getStopCount() const;
    /**
     * @brief Set StopCount (Modbus write, web "Reset Cycles"); RAM only until saved.
     */
    void setStopCount(uint32_t count);
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
    T read(const T& field) const;

    /**
     * @brief Set one value under the mutex.
     */
    template <typename T>
    void write(T& field, T value);

    mutable std::mutex mu;   ///< web/modbus write while the gate reads
    AdvancedValues values;   ///< all saved settings
    uint32_t stopCount = 0;  ///< was the static _StopCount
};

#endif // CONFIG_ADVANCED_SETTINGS_H
