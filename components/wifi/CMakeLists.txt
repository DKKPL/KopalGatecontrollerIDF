# Soft AP with DHCP and wildcard DNS, switched by the Wi-Fi switch.
idf_component_register(
    SRCS "src/wifi_manager.cpp"
         "src/soft_ap.cpp"
         "src/station.cpp"
         "src/captive_dns.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_wifi esp_netif esp_event lwip app_controller config hal_io motor_control
)
