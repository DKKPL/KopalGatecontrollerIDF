idf_component_register(
    SRCS "src/config_storage.cpp"
         "src/factory_defaults.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES nvs_flash
)
