idf_component_register(
    SRCS "src/counter.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_pcnt
)
