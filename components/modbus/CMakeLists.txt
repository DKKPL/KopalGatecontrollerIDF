idf_component_register(
    SRCS "src/modbus_rtu.cpp"
         "src/modbus_registers.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES board config app_controller esp_driver_uart
)
