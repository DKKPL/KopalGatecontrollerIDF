# Configuration web pages: public settings, advanced settings, movement trace.
idf_component_register(
    SRCS "src/web_server.cpp"
         "src/pages.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES config app_controller motor_control esp_http_server
    EMBED_FILES "assets/logo.jpg"
)
