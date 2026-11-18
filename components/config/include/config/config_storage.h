#ifndef CONFIG_CONFIG_STORAGE_H
#define CONFIG_CONFIG_STORAGE_H

#include <cstddef>
#include <cstdint>

class AdvancedSettings;
class PublicSettings;
class TeachSettings;

/**
 * @brief Keeps the settings in NVS flash, one blob per settings class (was ConfigurationStore).
 */
class ConfigStorage {
public:
    /**
     * @brief Start NVS, open the settings namespace and check the build stamp; call once at startup, before Wi-Fi (new).
     * @return false when NVS cannot be used; loads then fail and the settings keep their defaults
     */
    bool begin();

    /**
     * @brief The firmware build stamp; a change forces a re-teach (was FIRMWARE_BUILD_STAMP).
     */
    static const char* getFirmwareBuildStamp();

    /**
     * @brief Load all settings at startup; missing ones get their defaults and are saved (was Program.LoadConfigs).
     */
    void loadAll(PublicSettings& pub, AdvancedSettings& adv, TeachSettings& teach);

    /**
     * @brief Load the saved public settings (was IsConfigFileExisting + GetPublicConfig).
     * @return false when nothing usable is saved; pub is then unchanged
     */
    bool loadPublic(PublicSettings& pub);
    /**
     * @brief Load the saved advanced settings and stop count (was IsConfigFileExisting + GetAdvancedConfig).
     * @return false when nothing usable is saved; adv is then unchanged
     */
    bool loadAdvanced(AdvancedSettings& adv);
    /**
     * @brief Load the saved teach positions (was IsConfigFileExisting + GetTeachConfig).
     * @return false when nothing usable is saved; teach is then unchanged
     */
    bool loadTeach(TeachSettings& teach);

    /**
     * @brief Save the public settings (was WritePublicConfig).
     */
    bool savePublic(const PublicSettings& pub);
    /**
     * @brief Save the advanced settings and the stop count (was WriteAdvancedConfig).
     */
    bool saveAdvanced(const AdvancedSettings& adv);
    /**
     * @brief Save the teach positions now (was WriteTeachConfig + writeTeach).
     */
    bool saveTeach(const TeachSettings& teach);
    /**
     * @brief Save only the stop count; the gate calls it after every stop (new).
     */
    bool saveStopCount(uint32_t count);

    /**
     * @brief Delete the saved public settings (was ClearConfig(typeof(PublicSettings))).
     */
    void clearPublic();
    /**
     * @brief Delete the saved advanced settings and stop count (was ClearConfig(typeof(AdvancedSettings))).
     */
    void clearAdvanced();
    /**
     * @brief Delete the saved teach positions; the gate must be taught again (was ClearConfig(typeof(TeachSettings))).
     */
    void clearTeach();

private:
    /**
     * @brief Read a blob of exactly size bytes; false when missing, another size or unreadable.
     */
    bool readBlob(const char* key, void* data, size_t size);
    /**
     * @brief Write a blob and commit it.
     */
    bool writeBlob(const char* key, const void* data, size_t size);
    /**
     * @brief Delete a key and commit; a missing key is fine.
     */
    void eraseKey(const char* key);

    /**
     * @brief Clear all saved settings when the firmware build stamp changed; forces a re-teach (was the start of LoadConfigs).
     */
    void checkBuildStamp();

    uint32_t handle = 0;   ///< open NVS handle (nvs_handle_t)
    bool ready = false;    ///< begin() succeeded
};

#endif // CONFIG_CONFIG_STORAGE_H
