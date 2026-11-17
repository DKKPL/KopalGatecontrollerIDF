idf_component_register(
    SRCS "src/wifi_manager.cpp"
         "src/captive_dns.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_wifi esp_netif esp_event lwip
)
