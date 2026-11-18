idf_component_register(
    SRCS "src/hal_motor.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES board config esp_driver_ledc esp_driver_gpio
)
