idf_component_register(
    SRCS "src/hal_counter.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES board esp_driver_pcnt
)
