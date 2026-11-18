#include "wifi/wifi_manager.h"

#include <cstring>

#include "config/public_settings.h"
#include "dhcpserver/dhcpserver.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "hal_io/hal_io.h"
#include "motor_control/time_util.h"

static const char* TAG = "wifi";

WifiManager::WifiManager(Io& io, const PublicSettings& pub) : io(io), pub(pub)
{
}

void WifiManager::setWebServerStarter(std::function<void()> starter)
{
    startWebServer = std::move(starter);
}

bool WifiManager::start()
{
    esp_err_t err = esp_netif_init();
    if (err == ESP_OK) {
        err = esp_event_loop_create_default();
        if (err == ESP_ERR_INVALID_STATE) {
            err = ESP_OK;   // already created by another component
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Network stack init failed: %s", esp_err_to_name(err));
        return false;
    }

    netif = esp_netif_create_default_wifi_ap();
    if (netif == nullptr) {
        ESP_LOGE(TAG, "Creating the access point interface failed");
        return false;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err == ESP_OK) {
        err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    }
    if (err == ESP_OK) {
        err = esp_wifi_set_mode(WIFI_MODE_AP);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi driver init failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

void WifiManager::applyWifiSwitch()
{
    // The switch pulls the pin low when it is on (was _wifiPinIn.Read() == PinValue.Low)
    if (kForceWifiOnForDebug || !io.isHigh(Input::Wifi)) {
        enableWifi();
    } else {
        disableWifi();
    }
}

void WifiManager::enableWifi()
{
    if (!wifiEnable) {
        if (!startAccessPoint()) {
            return;
        }
        wifiEnable = true;
        io.write(Output::UserLed, true);
        wifiOnSinceMs = nowMs();
    }
    // this is a function pointer
    // The web side ignores a second start (C#: only when the thread was not alive)
    if (startWebServer) {
        startWebServer();
    }
}

void WifiManager::disableWifi()
{
    io.write(Output::UserLed, false);
    if (!wifiEnable) {
        return;
    }
    wifiEnable = false;

    const esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Stopping the access point failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Access point off");
}

void WifiManager::switchOffAfterTimeout()
{
    // Disable Wi-Fi only when the gate is completely closed and Wi-Fi has been on for more than 5 minutes.
    if (wifiEnable && secondsSince(wifiOnSinceMs) > kWifiOnMaxSeconds) {
        disableWifi();
    }
}

bool WifiManager::startAccessPoint()
{
    esp_ip4_addr_t address = {};
    esp_netif_str_to_ip4(kIp, &address);

    // Static address with ourselves as gateway; the DHCP server must be stopped to change it.
    esp_netif_dhcps_stop(netif);
    esp_netif_ip_info_t ipInfo = {};
    ipInfo.ip = address;
    ipInfo.gw = address;
    esp_netif_str_to_ip4(kNetmask, &ipInfo.netmask);
    esp_err_t err = esp_netif_set_ip_info(netif, &ipInfo);

    // DHCP offers our address as gateway and DNS server, so phones ask us for every name.
    if (err == ESP_OK) {
        dhcps_offer_t offer = OFFER_ROUTER;
        err = esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_ROUTER_SOLICITATION_ADDRESS, &offer, sizeof(offer));
    }
    if (err == ESP_OK) {
        dhcps_offer_t offer = OFFER_DNS;
        err = esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &offer, sizeof(offer));
    }
    if (err == ESP_OK) {
        esp_netif_dns_info_t dnsInfo = {};
        dnsInfo.ip.type = ESP_IPADDR_TYPE_V4;
        dnsInfo.ip.u_addr.ip4 = address;
        err = esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dnsInfo);
    }
    if (err == ESP_OK) {
        err = esp_netif_dhcps_start(netif);
        if (err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
            err = ESP_OK;
        }
    }

    wifi_config_t config = {};
    std::memcpy(config.ap.ssid, kSsid, std::strlen(kSsid));
    config.ap.ssid_len = static_cast<uint8_t>(std::strlen(kSsid));
    std::memcpy(config.ap.password, kPassword, std::strlen(kPassword));
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    config.ap.max_connection = kMaxConnections;
    config.ap.channel = kChannel;
    config.ap.beacon_interval = 100;
    if (err == ESP_OK) {
        err = esp_wifi_set_config(WIFI_IF_AP, &config);
    }
    if (err == ESP_OK) {
        err = esp_wifi_start();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Starting the access point failed: %s", esp_err_to_name(err));
        return false;
    }

    // Captive portal: every name resolves to us, so phones open the settings page by themselves.
    if (pub.getAutoRedirectEnabled()) {
        dns.start(address.addr);
    }

    ESP_LOGI(TAG, "Access point '%s' on %s", kSsid, kIp);
    return true;
}

// Wireless80211 (station mode): not ported, the firmware never joined another network.
