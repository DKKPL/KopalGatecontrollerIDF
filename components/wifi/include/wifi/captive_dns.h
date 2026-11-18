#ifndef WIFI_CAPTIVE_DNS_H
#define WIFI_CAPTIVE_DNS_H

#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Wildcard DNS server: answers every A query with the Soft AP address, so phones show
 *        the captive portal (was Iot.Device.DnsServer with DnsEntry("*", address)).
 */
class CaptiveDns {
public:
    /**
     * @brief Open UDP port 53 on address and answer in a task of its own (was DnsServer.Start).
     * @param address IPv4 address in network byte order (esp_ip4_addr_t.addr)
     * @return false when the socket or the task cannot be created
     */
    bool start(uint32_t address);

private:
    /**
     * @brief Stack of the DNS task in bytes.
     */
    static constexpr int kTaskStack = 4096;
    /**
     * @brief Priority of the DNS task.
     */
    static constexpr UBaseType_t kTaskPriority = 5;

    /**
     * @brief FreeRTOS entry point; calls run() on the server passed as arg.
     */
    static void taskEntry(void* arg);
    /**
     * @brief The DNS task: receive a query, send the answer, forever.
     */
    void run();

    int sock = -1;                ///< UDP socket on port 53
    uint32_t address = 0;         ///< answer for every name, network byte order
    TaskHandle_t task = nullptr;  ///< the DNS task, nullptr until start()
};

#endif // WIFI_CAPTIVE_DNS_H
