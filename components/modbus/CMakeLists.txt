idf_component_register(
    SRCS "src/modbus_rtu.cpp"
         "src/modbus_registers.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES board config motor_control esp_driver_uart
)
