idf_component_register(
    SRCS "src/io.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES board esp_driver_gpio
)
