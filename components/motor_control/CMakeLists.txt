idf_component_register(
    SRCS "src/gate_controller.cpp"
         "src/ramp_generator.cpp"
         "src/safety.cpp"
         "src/teach.cpp"
         "src/move_trace.cpp"
    INCLUDE_DIRS "include"
    REQUIRES config
    PRIV_REQUIRES hal_io hal_motor hal_counter esp_timer
)
