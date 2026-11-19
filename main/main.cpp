#include "app_controller/gate_controller.h"
#include "config/advanced_settings.h"
#include "config/config_storage.h"
#include "config/public_settings.h"
#include "config/teach_settings.h"
#include "esp_log.h"
#include "esp_system.h"
#include "hal_counter/hal_counter.h"
#include "hal_io/hal_io.h"
#include "hal_motor/hal_motor.h"
#include "modbus/modbus_rtu.h"
#include "web/web_server.h"
#include "wifi/wifi_manager.h"

static const char* TAG = "main";

// Log the free heap with a label (was LogFreeMemory).
static void logFreeMemory(const char* where)
{
    ESP_LOGI(TAG, "[MEM] %s: free=%u", where, static_cast<unsigned>(esp_get_free_heap_size()));
}

// --- Temporary: the template app_main, kept until mn.appMain is ported (the link needs an app_main). ---
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Hello from ESP-IDF");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
