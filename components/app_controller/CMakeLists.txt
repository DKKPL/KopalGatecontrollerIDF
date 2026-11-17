# The gate application: state machine, commands, lock/beacon/light outputs, teach.
idf_component_register(
    SRCS "src/gate_controller.cpp"
         "src/gate_commands.cpp"
         "src/gate_outputs.cpp"
         "src/teach.cpp"
    INCLUDE_DIRS "include"
    REQUIRES motor_control
    PRIV_REQUIRES config hal_io hal_motor hal_counter
)
