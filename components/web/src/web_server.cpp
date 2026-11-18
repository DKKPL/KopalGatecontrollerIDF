#include "web/web_server.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "app_controller/gate_controller.h"
#include "app_controller/gate_types.h"
#include "config/advanced_settings.h"
#include "config/config_storage.h"
#include "config/public_settings.h"
#include "config/teach_settings.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "motor_control/time_util.h"

static const char* TAG = "web";

static constexpr const char* kAdminUser = "KopalAdmin";
static constexpr const char* kAdminPass = "8610!";

// Known OS/browser captive-portal probe URLs. Any GET to one of these
// (or anything not matching "/", "/advanced" or "/trace") triggers the redirect
// that pops up the captive-portal page on the client device.
static constexpr const char* kCommonRedirects[] = {
    "/generate_204",               // Android
    "/fwlink",                     // Apple
    "/hotspot-detect.html",        // Apple iOS
    "/ncsi.txt",                   // Windows
    "/connecttest.txt",            // Windows
    "/redirect",                   // Microsoft
    "/library/test/success.html",  // Google
    "/canonical.html",             // Mozilla Firefox
    "/success.txt",                // Mozilla Firefox
};

// Pages are sent in chunks of this size (see sendPage).
static constexpr size_t kChunkSize = 1024;

// A form body larger than this is refused (the advanced form is about 1.5 KB).
static constexpr size_t kMaxBodyBytes = 8192;

void WebServer::setPublicSavedHandler(std::function<void()> handler)
{
    onPublicSaved = std::move(handler);
}

// The logo JPEG, embedded by CMakeLists.txt (EMBED_FILES "assets/logo.jpg") (was LogoData.Bytes).
extern const uint8_t logoJpgStart[] asm("_binary_logo_jpg_start");
extern const uint8_t logoJpgEnd[] asm("_binary_logo_jpg_end");

// A request header, or "" when it is missing.
static std::string getHeader(httpd_req_t* req, const char* name)
{
    const size_t len = httpd_req_get_hdr_value_len(req, name);
    if (len == 0) {
        return "";
    }
    std::string value(len + 1, '\0');
    if (httpd_req_get_hdr_value_str(req, name, &value[0], len + 1) != ESP_OK) {
        return "";
    }
    value.resize(len);
    return value;
}

WebServer::WebServer(PublicSettings& pub, AdvancedSettings& adv, const TeachSettings& teach,
                     ConfigStorage& store, GateController& gate, const char* softApIp)
    : pub(pub), adv(adv), teach(teach), store(store), gate(gate)
{
    this->softApIp = softApIp;
}

bool WebServer::start()
{
    // Never create a second instance.
    if (server != nullptr) {
        return true;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 8192;
    config.lru_purge_enable = true;                    // many probe connections at once: drop the oldest
    config.uri_match_fn = httpd_uri_match_wildcard;    // "/*": every URL comes to onRequest, like CommandReceived

    httpd_handle_t handle = nullptr;
    bool started = httpd_start(&handle, &config) == ESP_OK;

    if (started) {
        httpd_uri_t get = {};
        get.uri = "/*";
        get.method = HTTP_GET;
        get.handler = &WebServer::onRequest;
        get.user_ctx = this;

        httpd_uri_t post = get;
        post.method = HTTP_POST;

        started = httpd_register_uri_handler(handle, &get) == ESP_OK
               && httpd_register_uri_handler(handle, &post) == ESP_OK;
        if (started) {
            server = handle;
        } else {
            httpd_stop(handle);
        }
    }

    if (started) {
        ESP_LOGI(TAG, "Web server running on port 80.");
    } else {
        ESP_LOGE(TAG, "Web server FAILED to start on port 80.");
    }
    return started;
}

void WebServer::stop()
{
    if (server == nullptr) {
        return;
    }
    httpd_stop(server);
    server = nullptr;
}

esp_err_t WebServer::onRequest(httpd_req_t* req)
{
    WebServer* self = static_cast<WebServer*>(req->user_ctx);

    ESP_LOGD(TAG, "[MEM] request start: %lu", static_cast<unsigned long>(esp_get_free_heap_size()));

    const std::string url = req->uri;
    ESP_LOGI(TAG, "%s -> %s", http_method_str(static_cast<http_method>(req->method)), url.c_str());

    // Strip query string for URL comparisons so probe URLs like
    // /generate_204?ts=123 still match the expected path
    const size_t qmarkIdx = url.find('?');
    const std::string urlPath = qmarkIdx != std::string::npos && qmarkIdx > 0 ? url.substr(0, qmarkIdx) : url;

    if (req->method == HTTP_GET) {
        return self->handleGet(req, urlPath);
    }
    if (req->method == HTTP_POST) {
        return self->handlePost(req, urlPath);
    }
    return ESP_OK;
}

esp_err_t WebServer::handleGet(httpd_req_t* req, const std::string& urlPath)
{
    if (urlPath == "/favicon.ico") {
        return sendEmpty(req, HTTPD_404);
    }

    if (urlPath == "/logo.jpg") {
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_set_type(req, "image/jpeg");
        httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
        return httpd_resp_send(req, reinterpret_cast<const char*>(logoJpgStart), logoJpgEnd - logoJpgStart);
    }

    bool isKnownProbe = false;
    for (const char* probe : kCommonRedirects) {
        if (urlPath == probe) {
            isKnownProbe = true;
        }
    }
    const bool isCatchAll = urlPath != "/" && urlPath != "/advanced" && urlPath != "/trace";

    if (isKnownProbe || isCatchAll) {
        if (pub.getAutoRedirectEnabled()) {
            const std::string location = "http://" + softApIp + "/";
            return redirect(req, location.c_str());
        }
        // Portal disabled: answer like a normal network so the OS doesn't pop up the page.
        return sendEmpty(req, urlPath == "/generate_204" ? HTTPD_204 : HTTPD_404);
    }

    if (urlPath == "/advanced") {
        if (!isAuthenticated(req)) {
            return sendUnauthorizedResponse(req);
        }

        ESP_LOGD(TAG, "[MEM] free before advanced page: %lu", static_cast<unsigned long>(esp_get_free_heap_size()));
        return sendPage(req, "text/html", [this](const PartSink& s) { pages::writeAdvancedPage(s, adv); });
    }

    if (urlPath == "/trace") {
        if (!isAuthenticated(req)) {
            return sendUnauthorizedResponse(req);
        }

        // Collect first: the trace holds its lock while writing, and the gate must not wait for the network.
        std::string csv;
        csv.reserve(4096);
        pages::writeTrace([&csv](const char* part) { csv += part; }, adv, teach, gate.getTrace());
        return sendPage(req, "text/plain", [&csv](const PartSink& s) { s(csv.c_str()); });
    }

    // "/" - public settings portal page
    ESP_LOGD(TAG, "[MEM] free before public page: %lu", static_cast<unsigned long>(esp_get_free_heap_size()));
    return sendPage(req, "text/html", [this](const PartSink& s) { pages::writePublicPage(s, pub, adv, teach); });
}

esp_err_t WebServer::handlePost(httpd_req_t* req, const std::string& urlPath)
{
    if (urlPath == "/advanced" && !isAuthenticated(req)) {
        return sendUnauthorizedResponse(req);
    }

    const std::string contentType = getHeader(req, "Content-Type");
    if (!contentType.starts_with("application/x-www-form-urlencoded")) {
        return sendEmpty(req, HTTPD_400);
    }

    const size_t contentLength = req->content_len;
    if (contentLength == 0) {
        return sendEmpty(req, HTTPD_400);
    }

    std::string formData;
    if (!readRequestBody(req, contentLength, formData)) {
        return sendEmpty(req, HTTPD_500);
    }

    if (urlPath == "/advanced") {
        processAdvancedPost(formData);
    } else if (!processPublicPost(formData)) {
        return sendEmpty(req, HTTPD_500);   // C#: int.Parse threw, the catch answered 500
    }

    // Free the request buffer before building the response.
    formData.clear();
    formData.shrink_to_fit();

    esp_err_t err;
    if (urlPath == "/advanced") {
        err = sendPage(req, "text/html", [this](const PartSink& s) { pages::writeAdvancedPage(s, adv); });
    } else {
        err = sendPage(req, "text/html", [this](const PartSink& s) { pages::writePublicPage(s, pub, adv, teach); });
    }

    if (rebootAfterResponse) {
        ESP_LOGI(TAG, "Encoder mode changed -> rebooting");
        sleepMs(1000);   // let the response reach the browser
        esp_restart();
    }
    return err;
}

void WebServer::setNoCacheHeaders(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
}

esp_err_t WebServer::redirect(httpd_req_t* req, const char* location)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", location);
    setNoCacheHeaders(req);
    return httpd_resp_send(req, "", 0);
}

esp_err_t WebServer::sendEmpty(httpd_req_t* req, const char* status)
{
    httpd_resp_set_status(req, status);
    return httpd_resp_send(req, "", 0);
}

esp_err_t WebServer::sendUnauthorizedResponse(httpd_req_t* req)
{
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Advanced Settings\"");
    httpd_resp_set_type(req, "text/plain");
    setNoCacheHeaders(req);
    return httpd_resp_sendstr(req, "401 Unauthorized");
}

// Collects page parts and sends them as HTTP chunks of up to kChunkSize bytes.
class ChunkSender {
public:
    explicit ChunkSender(httpd_req_t* req)
    {
        this->req = req;
    }

    // Add a part; sends a chunk each time the buffer is full. After an error everything is dropped.
    void add(const char* part)
    {
        size_t len = strlen(part);
        while (len > 0 && err == ESP_OK) {
            const size_t n = std::min(len, sizeof(buf) - used);
            memcpy(buf + used, part, n);
            used += n;
            part += n;
            len -= n;
            if (used == sizeof(buf)) {
                flush();
            }
        }
    }

    // Send what is left and the end-of-response chunk.
    esp_err_t finish()
    {
        flush();
        if (err == ESP_OK) {
            err = httpd_resp_send_chunk(req, nullptr, 0);
        }
        return err;
    }

private:
    void flush()
    {
        if (used > 0 && err == ESP_OK) {
            err = httpd_resp_send_chunk(req, buf, static_cast<ssize_t>(used));
        }
        used = 0;
    }

    httpd_req_t* req = nullptr;
    char buf[kChunkSize];
    size_t used = 0;
    esp_err_t err = ESP_OK;
};

esp_err_t WebServer::sendPage(httpd_req_t* req, const char* contentType, const PageWriter& writer)
{
    httpd_resp_set_status(req, HTTPD_200);
    httpd_resp_set_type(req, contentType);
    setNoCacheHeaders(req);

    // Stream each part directly, so no single large allocation is needed.
    ChunkSender out(req);
    writer([&out](const char* part) { out.add(part); });
    return out.finish();
}

bool WebServer::readRequestBody(httpd_req_t* req, size_t contentLength, std::string& body)
{
    if (contentLength == 0 || contentLength > kMaxBodyBytes) {
        return false;
    }

    body.resize(contentLength);
    size_t total = 0;

    while (total < contentLength) {
        const int read = httpd_req_recv(req, &body[total], contentLength - total);

        if (read <= 0) {
            break;
        }

        total += static_cast<size_t>(read);
    }

    return total == contentLength;
}

bool WebServer::processPublicPost(const std::string& formData)
{
    PublicValues v = pub.getValues();
    if (!pages::readPublicForm(formData, v)) {
        return false;   // C#: int.Parse threw; nothing is applied
    }

    pub.setValues(v);
    store.savePublic(pub);
    if (onPublicSaved) {
        onPublicSaved();   // was Program.ApplyModbusAddress()
    }
    return true;
}

void WebServer::processAdvancedPost(const std::string& formData)
{
    ESP_LOGI(TAG, "ProcessAdvancedPost called");
    ESP_LOGI(TAG, "formData: %s", formData.c_str());

    // Reset to Defaults: Terminus defaults; the stop count stays.
    if (formData.find("reset=1") != std::string::npos) {
        const bool previousUseQuadrature = adv.getUseQuadratureEncoder();
        adv.resetToDefaults();
        saveAdvanced();

        // Program.ResetPulseCounter() is not called: the encoder mode change reboots instead.
        if (adv.getUseQuadratureEncoder() != previousUseQuadrature) {
            rebootAfterResponse = true;
        }
        sleepMs(500);
        return;
    }

    // Reset Cycles
    if (formData.find("reset=2") != std::string::npos) {
        adv.setStopCount(0);
        saveAdvanced();
        sleepMs(500);
        return;
    }

    // Edit a copy and set it back in one go, so the gate never sees half-read values.
    AdvancedValues v = adv.getValues();
    const bool previousUseQuadrature = v.useQuadratureEncoder;
    pages::readAdvancedForm(formData, v);
    adv.setValues(v);
    saveAdvanced();

    // The pulse counter is not reset (Program.ResetPulseCounter() was commented out):
    // a changed encoder mode reboots after the answer, so the counter starts with the right pins.
    if (v.useQuadratureEncoder != previousUseQuadrature) {
        rebootAfterResponse = true;
    }
}

void WebServer::saveAdvanced()
{
    store.saveAdvanced(adv);
    if (!gate.post(GateCommand::ApplySettings)) {   // was Program.ApplyUseMotorDisableSetting()
        ESP_LOGW(TAG, "Gate command queue full: motor disable setting not applied");
    }
}

// Value of one Base64 character, -1 when c is not one.
static int base64Value(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '+') {
        return 62;
    }
    if (c == '/') {
        return 63;
    }
    return -1;
}

// Standard Base64 with '=' padding (was Convert.FromBase64String). false when the text is not valid.
static bool decodeBase64(const std::string& in, std::string& out)
{
    out.clear();
    if (in.size() % 4 != 0) {
        return false;
    }
    uint32_t acc = 0;
    int bits = 0;
    size_t padding = 0;
    for (const char c : in) {
        if (c == '=') {
            ++padding;
            continue;
        }
        const int value = base64Value(c);
        if (value < 0 || padding > 0) {   // not Base64, or data after the padding
            return false;
        }
        acc = (acc << 6) | static_cast<uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += static_cast<char>((acc >> bits) & 0xFF);
        }
    }
    return padding <= 2;
}

bool WebServer::isAuthenticated(httpd_req_t* req)
{
    const std::string authHeader = getHeader(req, "Authorization");
    if (authHeader.empty()) {
        return false;
    }

    static constexpr const char* authPrefix = "Basic ";
    if (!authHeader.starts_with(authPrefix)) {
        return false;
    }

    std::string decoded;
    if (!decodeBase64(authHeader.substr(strlen(authPrefix)), decoded)) {
        return false;
    }

    const size_t colon = decoded.find(':');
    if (colon == std::string::npos || decoded.find(':', colon + 1) != std::string::npos) {
        return false;
    }

    return decoded.substr(0, colon) == kAdminUser && decoded.substr(colon + 1) == kAdminPass;
}
