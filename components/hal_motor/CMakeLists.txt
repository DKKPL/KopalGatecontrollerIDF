idf_component_register(
    SRCS "src/motor.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES board esp_driver_ledc esp_driver_gpio
)
