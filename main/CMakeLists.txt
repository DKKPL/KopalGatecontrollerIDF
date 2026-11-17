idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES config app_controller modbus wifi web
)
