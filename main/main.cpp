#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "main";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Hello from ESP-IDF");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}