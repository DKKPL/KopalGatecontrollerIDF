# Header-only component: GPIO assignments for the Kopal gate controller board.
idf_component_register(
    INCLUDE_DIRS "include"
    REQUIRES esp_hal_gpio
)
