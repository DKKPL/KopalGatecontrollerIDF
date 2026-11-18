#ifndef WIFI_WIFI_MANAGER_H
#define WIFI_WIFI_MANAGER_H

#include <cstdint>
#include <functional>

#include "wifi/captive_dns.h"

class Io;
class PublicSettings;
struct esp_netif_obj;

/**
 * @brief The Wi-Fi access point for the settings pages: on when the Wi-Fi switch is on,
 *        off when it is off or 5 min after Wi-Fi went on, once the gate is closed
 *        (was the WiFi region of Program and WirelessAP).
 */
class WifiManager {
public:
    /**
     * @brief Address of the access point; the settings pages are at http://192.168.4.1 (was WirelessAP.SoftApIP).
     */
    static constexpr const char* kIp = "192.168.4.1";

    /**
     * @brief Keep the Io and the settings; nothing starts yet.
     */
    WifiManager(Io& io, const PublicSettings& pub);

    /**
     * @brief Set up the Wi-Fi driver once at startup, after NVS (ConfigStorage::begin); the AP stays off (new).
     * @return false when the Wi-Fi driver cannot be set up
     */
    bool start();

    /**
     * @brief Function that starts the web server; called each time Wi-Fi goes on (was _webServerThread).
     */
    void setWebServerStarter(std::function<void()> starter);

    /**
     * @brief Read the Wi-Fi switch and switch the AP on or off (was PollWifiButton).
     *        The gate calls it at every entry point: at boot and after an ops-limit restart.
     */
    void applyWifiSwitch();

    /**
     * @brief Gate fully closed: switch Wi-Fi off when it has been on for more than 5 minutes
     *        (was the Wi-Fi block in State_Closing_normal_stop).
     */
    void switchOffAfterTimeout();

private:
    static constexpr const char* kSsid = "KopalGateController";  ///< network name (was SoftApSsid)
    static constexpr const char* kPassword = "KopalKGC";         ///< WPA2 password, at least 8 characters
    static constexpr const char* kNetmask = "255.255.255.0";     ///< subnet of the access point
    static constexpr uint8_t kChannel = 11;                      ///< nanoFramework's default channel
    static constexpr uint8_t kMaxConnections = 4;                ///< phones open several connections at once
    static constexpr int kWifiOnMaxSeconds = 5 * 60;             ///< Wi-Fi off after this, once the gate is closed
    static constexpr bool kForceWifiOnForDebug = false;          ///< DEBUG ONLY: act as if the switch is on (was ForceWifiOnForDebug)

    /**
     * @brief Start the AP once, light the LED, start the 5 min watch and the web server (was EnableWifi).
     */
    void enableWifi();

    /**
     * @brief Switch the AP and the LED off (was DisableWifi).
     */
    void disableWifi();

    /**
     * @brief Configure and start the access point, its DHCP server and the captive DNS (was WirelessAP).
     * @return false when the access point could not be started
     */
    bool startAccessPoint();

    Io& io;
    const PublicSettings& pub;
    CaptiveDns dns;                        ///< wildcard DNS for the captive portal
    std::function<void()> startWebServer;  ///< set by main.cpp (was _webServerThread)
    esp_netif_obj* netif = nullptr;        ///< the access point interface (esp_netif_t)
    bool wifiEnable = false;               ///< the AP is on (was bWifiEnable)
    int64_t wifiOnSinceMs = 0;             ///< when Wi-Fi went on (was _wifiWatch)
};

#endif // WIFI_WIFI_MANAGER_H
