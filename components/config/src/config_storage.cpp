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
static constexpr const char* kFirmwareBuildStamp = "2026-11-14-0005";

// NVS namespace and keys (max 15 characters each)
static constexpr const char* kNamespace = "config";
static constexpr const char* kKeyPublic = "public";
static constexpr const char* kKeyAdvanced = "advanced";
static constexpr const char* kKeyTeach = "teach";
static constexpr const char* kKeyStopCount = "stopCount";
static constexpr const char* kKeyBuildStamp = "buildStamp";
