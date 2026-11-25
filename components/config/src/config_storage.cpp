#include "config/config_storage.h"

#include <cstring>

#include "config/advanced_settings.h"
#include "config/public_settings.h"
#include "config/teach_settings.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char* TAG = "config";

// Bump this string with every deployment where you want a forced re-teach.
// Any change to this value clears the teach, advanced and public settings on the next boot.
static constexpr const char* kFirmwareBuildStamp = "2026-11-25-0002";

// NVS namespace and keys (max 15 characters each)
static constexpr const char* kNamespace = "config";
static constexpr const char* kKeyPublic = "public";
static constexpr const char* kKeyAdvanced = "advanced";
static constexpr const char* kKeyTeach = "teach";
static constexpr const char* kKeyStopCount = "stopCount";
static constexpr const char* kKeyBuildStamp = "buildStamp";

bool ConfigStorage::begin()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Partition full or written by another IDF version: erase it, all settings go back to defaults
        ESP_LOGW(TAG, "NVS unusable (%s), erasing it", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return false;
    }

    nvs_handle_t nvs = 0;
    err = nvs_open(kNamespace, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Opening NVS namespace failed: %s", esp_err_to_name(err));
        return false;
    }
    handle = nvs;
    ready = true;

    // Stored settings from another deployment must not be used: check before anything is loaded.
    checkBuildStamp();
    return true;
}

void ConfigStorage::checkBuildStamp()
{
    // --- New-deployment detection: a changed kFirmwareBuildStamp forces a re-teach and fresh settings ---
    char previousStamp[32] = "";
    size_t length = sizeof(previousStamp);
    if (ready) {
        const esp_err_t err = nvs_get_str(handle, kKeyBuildStamp, previousStamp, &length);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGD(TAG, "[CONFIG] Failed reading build stamp: %s", esp_err_to_name(err));
        }
        if (err != ESP_OK) {
            previousStamp[0] = '\0';
        }
    }

    const bool newFirmwareDetected = strcmp(previousStamp, kFirmwareBuildStamp) != 0;

    ESP_LOGD(TAG, "[CONFIG] Firmware stamp check: previous='%s', current='%s', newFirmwareDetected=%d",
             previousStamp, kFirmwareBuildStamp, newFirmwareDetected);

    if (newFirmwareDetected) {
        ESP_LOGD(TAG, "[CONFIG] New firmware detected (stamp changed: '%s' -> '%s'). Clearing TeachSettings "
                 "for re-teach and AdvancedSettings for fresh defaults.", previousStamp, kFirmwareBuildStamp);
        clearTeach();
        clearAdvanced();
        clearPublic();

        esp_err_t err = ready ? nvs_set_str(handle, kKeyBuildStamp, kFirmwareBuildStamp) : ESP_ERR_INVALID_STATE;
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        if (err != ESP_OK) {
            ESP_LOGD(TAG, "[CONFIG] Failed writing build stamp: %s", esp_err_to_name(err));
        }
    }
    // --- End new-deployment detection ---
}

bool ConfigStorage::readBlob(const char* key, void* data, size_t size)
{
    if (!ready) {
        return false;
    }
    // One read. NVS refuses a stored blob that is bigger than size (INVALID_LENGTH) and
    // copies a smaller one; either way length then holds the stored size.
    // The callers read into a temporary, so a partly filled data is never used.
    size_t length = size;
    const esp_err_t err = nvs_get_blob(handle, key, data, &length);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return false;
    }
    if (err == ESP_ERR_NVS_INVALID_LENGTH || (err == ESP_OK && length != size)) {
        // Saved by firmware with another settings layout
        ESP_LOGW(TAG, "%s has %u bytes, expected %u: not used", key,
                 static_cast<unsigned>(length), static_cast<unsigned>(size));
        return false;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Reading %s failed: %s", key, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool ConfigStorage::writeBlob(const char* key, const void* data, size_t size)
{
    if (!ready) {
        ESP_LOGE(TAG, "Saving %s failed: NVS not started", key);
        return false;
    }
    esp_err_t err = nvs_set_blob(handle, key, data, size);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Saving %s failed: %s", key, esp_err_to_name(err));
        return false;
    }
    return true;
}

void ConfigStorage::eraseKey(const char* key)
{
    if (!ready) {
        return;
    }
    esp_err_t err = nvs_erase_key(handle, key);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Deleting %s failed: %s", key, esp_err_to_name(err));
    }
}

void ConfigStorage::clearPublic()
{
    eraseKey(kKeyPublic);
}

void ConfigStorage::clearAdvanced()
{
    eraseKey(kKeyAdvanced);
    eraseKey(kKeyStopCount);
}

void ConfigStorage::clearTeach()
{
    eraseKey(kKeyTeach);
}

bool ConfigStorage::loadPublic(PublicSettings& pub)
{
    PublicValues values;
    if (!readBlob(kKeyPublic, &values, sizeof(values))) {
        return false;
    }
    pub.setValues(values);
    return true;
}

bool ConfigStorage::loadAdvanced(AdvancedSettings& adv)
{
    AdvancedValues values;
    if (!readBlob(kKeyAdvanced, &values, sizeof(values))) {
        return false;
    }
    adv.setValues(values);

    uint32_t count = 0;
    if (nvs_get_u32(handle, kKeyStopCount, &count) == ESP_OK) {
        adv.setStopCount(count);
    }
    return true;
}

bool ConfigStorage::loadTeach(TeachSettings& teach)
{
    TeachValues values;
    if (!readBlob(kKeyTeach, &values, sizeof(values))) {
        return false;
    }
    teach.setValues(values);
    return true;
}

bool ConfigStorage::savePublic(const PublicSettings& pub)
{
    const PublicValues values = pub.getValues();
    return writeBlob(kKeyPublic, &values, sizeof(values));
}

bool ConfigStorage::saveAdvanced(const AdvancedSettings& adv)
{
    const AdvancedValues values = adv.getValues();
    const bool ok = writeBlob(kKeyAdvanced, &values, sizeof(values));
    return saveStopCount(adv.getStopCount()) && ok;
}

bool ConfigStorage::saveTeach(const TeachSettings& teach)
{
    const TeachValues values = teach.getValues();
    return writeBlob(kKeyTeach, &values, sizeof(values));
}

bool ConfigStorage::saveStopCount(uint32_t count)
{
    if (!ready) {
        ESP_LOGE(TAG, "Saving %s failed: NVS not started", kKeyStopCount);
        return false;
    }
    esp_err_t err = nvs_set_u32(handle, kKeyStopCount, count);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Saving %s failed: %s", kKeyStopCount, esp_err_to_name(err));
        return false;
    }
    return true;
}

void ConfigStorage::loadAll(PublicSettings& pub, AdvancedSettings& adv, TeachSettings& teach)
{
    if (!loadPublic(pub)) {
        pub.setValues(PublicValues());
        savePublic(pub);
    }

    ESP_LOGD(TAG, "[CONFIG] Loaded PublicSettings: AutoRedirectEnabled=%d", pub.getAutoRedirectEnabled());

    if (!loadAdvanced(adv)) {
        adv.resetToDefaults();
        saveAdvanced(adv);
    }

    ESP_LOGD(TAG, "[CONFIG] Loaded persisted AdvancedSettings: USE_QUADRATURE_ENCODER=%d, "
             "INVERT_ENCODER_COUNT=%d, USE_DUAL_DIRECTION_INPUTS=%d",
             adv.getUseQuadratureEncoder(), adv.getInvertEncoderCount(), adv.getUseDualDirectionInputs());

    if (!loadTeach(teach)) {
        teach.setValues(TeachValues());
        saveTeach(teach);
    }
}

const char* ConfigStorage::getFirmwareBuildStamp()
{
    return kFirmwareBuildStamp;
}
