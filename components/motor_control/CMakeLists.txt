# How the motor moves: speed ramps, safety trips and backtrack, movement trace.
idf_component_register(
    SRCS "src/ramp_generator.cpp"
         "src/safety.cpp"
         "src/move_trace.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer
    PRIV_REQUIRES config hal_io hal_motor
)
