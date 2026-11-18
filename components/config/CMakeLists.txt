# Settings classes, factory defaults per gate model and their NVS storage.
idf_component_register(
    SRCS "src/advanced_settings.cpp"
         "src/config_storage.cpp"
         "src/factory_defaults.cpp"
         "src/public_settings.cpp"
         "src/teach_settings.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES nvs_flash
)
