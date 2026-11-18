idf_component_register(
    SRCS "src/hal_io.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES board esp_driver_gpio
)
