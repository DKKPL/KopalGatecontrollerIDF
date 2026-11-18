#include "wifi/captive_dns.h"

#include <cerrno>
#include <cstring>

#include "esp_log.h"
#include "lwip/sockets.h"

static const char* TAG = "captive_dns";

static constexpr uint16_t kDnsPort = 53;
static constexpr uint32_t kTtlSeconds = 300;   // as Iot.Device.DnsServer
static constexpr int kHeaderSize = 12;
static constexpr int kAnswerSize = 16;
static constexpr int kMaxPacket = 512;         // plain DNS over UDP
static constexpr uint16_t kTypeA = 1;

static uint16_t readU16(const uint8_t* p)
{
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

static void writeU16(uint8_t* p, uint16_t value)
{
    p[0] = static_cast<uint8_t>(value >> 8);
    p[1] = static_cast<uint8_t>(value & 0xFF);
}

// Turn the query in buf (len bytes, buffer kMaxPacket) into the answer, in place.
// Returns the answer length, or 0 to send nothing: like Iot.Device.DnsServer, only standard
// queries with one A question are answered; every name matches the "*" entry.
static int buildReply(uint8_t* buf, int len, uint32_t address)
{
    if (len < kHeaderSize) {
        return 0;
    }
    const uint16_t flags = readU16(buf + 2);
    const bool isResponse = (flags & 0x8000) != 0;
    const int opcode = (flags >> 11) & 0x0F;
    if (isResponse || opcode != 0 || readU16(buf + 4) != 1) {
        return 0;
    }

    // Question name: labels up to the 0 byte; compression pointers are not allowed here
    int pos = kHeaderSize;
    while (pos < len && buf[pos] != 0) {
        if ((buf[pos] & 0xC0) != 0) {
            return 0;
        }
        pos += buf[pos] + 1;
    }
    pos += 1;   // the 0 byte
    if (pos + 4 > len) {
        return 0;
    }
    const uint16_t type = readU16(buf + pos);
    const uint16_t qclass = readU16(buf + pos + 2);
    pos += 4;
    if (type != kTypeA || pos + kAnswerSize > kMaxPacket) {
        return 0;
    }

    // Header: response, authoritative, recursion-desired copied; one answer, nothing else
    writeU16(buf + 2, static_cast<uint16_t>(0x8000 | 0x0400 | (flags & 0x0100)));
    writeU16(buf + 6, 1);
    writeU16(buf + 8, 0);
    writeU16(buf + 10, 0);

    uint8_t* answer = buf + pos;
    writeU16(answer, static_cast<uint16_t>(0xC000 | kHeaderSize));   // name: the question name
    writeU16(answer + 2, kTypeA);
    writeU16(answer + 4, qclass);
    writeU16(answer + 6, static_cast<uint16_t>(kTtlSeconds >> 16));
    writeU16(answer + 8, static_cast<uint16_t>(kTtlSeconds & 0xFFFF));
    writeU16(answer + 10, 4);
    std::memcpy(answer + 12, &address, 4);   // already in network byte order
    return pos + kAnswerSize;
}

bool CaptiveDns::start(uint32_t address)
{
    if (task != nullptr) {
        return true;
    }
    this->address = address;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Creating the DNS socket failed: errno %d", errno);
        return false;
    }
    sockaddr_in bindAddr = {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(kDnsPort);
    bindAddr.sin_addr.s_addr = address;
    if (bind(sock, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) < 0) {
        ESP_LOGE(TAG, "Binding the DNS socket failed: errno %d", errno);
        close(sock);
        sock = -1;
        return false;
    }

    if (xTaskCreate(&CaptiveDns::taskEntry, "dns", kTaskStack, this, kTaskPriority, &task) != pdPASS) {
        ESP_LOGE(TAG, "Creating the DNS task failed");
        task = nullptr;
        close(sock);
        sock = -1;
        return false;
    }
    ESP_LOGI(TAG, "DNS server running, every name resolves to the Soft AP");
    return true;
}

void CaptiveDns::taskEntry(void* arg)
{
    static_cast<CaptiveDns*>(arg)->run();
}

void CaptiveDns::run()
{
    uint8_t buf[kMaxPacket];
    while (true) {
        sockaddr_in from = {};
        socklen_t fromLen = sizeof(from);
        const int len = recvfrom(sock, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (len < 0) {
            ESP_LOGW(TAG, "DNS receive failed: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        const int replyLen = buildReply(buf, len, address);
        if (replyLen > 0) {
            sendto(sock, buf, replyLen, 0, reinterpret_cast<sockaddr*>(&from), fromLen);
        }
    }
}
