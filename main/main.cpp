#include "app_controller/gate_controller.h"
#include "config/advanced_settings.h"
#include "config/config_storage.h"
#include "config/public_settings.h"
#include "config/teach_settings.h"
#include "esp_log.h"
#include "esp_system.h"
#include "hal_counter/hal_counter.h"
#include "hal_io/hal_io.h"
#include "hal_motor/hal_motor.h"
#include "modbus/modbus_rtu.h"
#include "web/web_server.h"
#include "wifi/wifi_manager.h"

static const char* TAG = "main";

// Log the free heap with a label (was LogFreeMemory).
static void logFreeMemory(const char* where)
{
    ESP_LOGI(TAG, "[MEM] %s: free=%u", where, static_cast<unsigned>(esp_get_free_heap_size()));
}

extern "C" void app_main(void)
{
    static ConfigStorage storage;
    static PublicSettings pub;
    static AdvancedSettings adv;
    static TeachSettings teach;
    static Io io;
    static Motor motor(adv);
    static Counter counter;
    static GateController gate(io, motor, counter, pub, adv, teach, storage);
    static ModbusSlave modbus(gate, io, pub, adv, storage);
    static WebServer web(pub, adv, teach, storage, gate, WifiManager::kIp);
    static WifiManager wifi(io, pub);

    logFreeMemory("Main start");
    ESP_LOGD(TAG, "ProtoTest Terminus");

    // Settings first: Wi-Fi needs NVS, and the pin setup depends on the settings (was LoadConfigs).
    storage.begin();
    storage.loadAll(pub, adv, teach);

    logFreeMemory("before SetDefaultGPIOStateCode");
    io.begin(adv.getUseQuadratureEncoder());
    motor.begin();
    logFreeMemory("after SetDefaultGPIOStateCode");

    // Wi-Fi starts the web server once the access point is up;
    // saving the public page re-applies the Modbus address (was Program.ApplyModbusAddress).
    web.setPublicSavedHandler([] { modbus.applyAddress(); });
    wifi.setWebServerStarter([] { web.start(); });
    wifi.start();

    // The gate reads the Wi-Fi switch at every (re)start of its state machine (was PollWifiButton in EntryPoint)
    // and lets wifi switch off a while after the gate closed.
    gate.setEntryPointHandler([] { wifi.applyWifiSwitch(); });
    gate.setCompleteCloseHandler([] { wifi.switchOffAfterTimeout(); });
    gate.start();

    // Modbus posts commands to the gate, so it starts after it (was InitAndStartModbusThread in EntryPoint).
    modbus.start();
}
