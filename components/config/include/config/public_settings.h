#ifndef CONFIG_PUBLIC_SETTINGS_H
#define CONFIG_PUBLIC_SETTINGS_H

#include <mutex>

/**
 * @brief The public (user) settings as plain values; saved as one NVS blob (was the PublicSettings properties).
 */
struct PublicValues {
    int autoCloseDelay = 20;              ///< s before the gate closes by itself (was AutoCloseDelay)
    float pedestrianModeDistance = 1.2f;  ///< pedestrian opening in meters (was PedestrianModeDistance)
    int passageLightDelay = 60;           ///< s the passage light stays on (was PassageLightDelay)
    int beaconPreWarningTime = 0;         ///< s the beacon flashes before moving (was BeaconPreWaringTime)
    int modbusAddress = 55;               ///< Modbus RTU slave address, 1-247 (was ModbusAddress)
    bool autoRedirectEnabled = true;      ///< captive-portal redirect on (was AutoRedirectEnabled)
};

/**
 * @brief Settings the user may change: delays, pedestrian opening, Modbus address (was PublicSettings).
 * Thread safe: every getter and setter takes the mutex for one copy.
 */
class PublicSettings {
public:
    /**
     * @brief Copy of all settings, taken under one lock (new).
     */
    PublicValues getValues() const;
    /**
     * @brief Replace all settings in one go (new).
     */
    void setValues(const PublicValues& values);

    /**
     * @brief Seconds before the gate closes by itself (was AutoCloseDelay).
     */
    int getAutoCloseDelay() const;
    /**
     * @brief Set AutoCloseDelay.
     */
    void setAutoCloseDelay(int seconds);
    /**
     * @brief Pedestrian opening in meters (was PedestrianModeDistance).
     */
    float getPedestrianModeDistance() const;
    /**
     * @brief Set PedestrianModeDistance.
     */
    void setPedestrianModeDistance(float meters);
    /**
     * @brief Pedestrian opening in encoder pulses (was PedestrianModeDistancePulse).
     * @param pulsesPerMeter AdvancedSettings::getPulsesPerMeter() (C# read a static copy of it)
     */
    int getPedestrianModeDistancePulse(double pulsesPerMeter) const;
    /**
     * @brief Seconds the passage light stays on (was PassageLightDelay).
     */
    int getPassageLightDelay() const;
    /**
     * @brief Set PassageLightDelay.
     */
    void setPassageLightDelay(int seconds);
    /**
     * @brief Seconds the beacon flashes before the gate moves (was BeaconPreWaringTime).
     */
    int getBeaconPreWarningTime() const;
    /**
     * @brief Set BeaconPreWaringTime.
     */
    void setBeaconPreWarningTime(int seconds);
    /**
     * @brief Modbus RTU slave address (was ModbusAddress).
     */
    int getModbusAddress() const;
    /**
     * @brief Set ModbusAddress; the caller checks the range 1-247, like C#.
     */
    void setModbusAddress(int address);
    /**
     * @brief Captive-portal redirect on (was AutoRedirectEnabled).
     */
    bool getAutoRedirectEnabled() const;
    /**
     * @brief Set AutoRedirectEnabled.
     */
    void setAutoRedirectEnabled(bool enabled);

private:
    /**
     * @brief Copy one value under the mutex.
     */
    template <typename T>
    T read(const T& field) const;

    /**
     * @brief Set one value under the mutex.
     */
    template <typename T>
    void write(T& field, const T& value);

    mutable std::mutex mu;   ///< web/modbus write while the gate reads
    PublicValues values;     ///< all saved settings
};

#endif // CONFIG_PUBLIC_SETTINGS_H
