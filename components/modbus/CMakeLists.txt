# Modbus RTU slave on the RS485 port (own implementation, not esp-modbus).
idf_component_register(
    SRCS "src/modbus_rtu.cpp"
         "src/modbus_registers.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES board config app_controller hal_io esp_driver_uart
)
