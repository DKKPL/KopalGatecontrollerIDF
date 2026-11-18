#ifndef WEB_WEB_SERVER_H
#define WEB_WEB_SERVER_H

#include <cstddef>
#include <functional>
#include <string>

#include "esp_err.h"
#include "web/pages.h"

class AdvancedSettings;
class ConfigStorage;
class GateController;
class PublicSettings;
class TeachSettings;
struct httpd_req;

/**
 * @brief The configuration web pages on port 80: public settings, advanced settings and the
 *        movement trace (was SimpleWebServer).
 */
class WebServer {
public:
    /**
     * @brief Settings, storage and gate are created in main.cpp and must outlive the server.
     * @param softApIp address of the access point, the captive-portal redirect target (was WirelessAP.SoftApIP)
     */
    WebServer(PublicSettings& pub, AdvancedSettings& adv, const TeachSettings& teach,
              ConfigStorage& store, GateController& gate, const char* softApIp);

    /**
     * @brief Start the server on port 80; does nothing when it already runs (was Start).
     * @return true when the server runs
     */
    bool start();
    /**
     * @brief Stop the server (new; C# never stopped it).
     */
    void stop();
    /**
     * @brief Function to call after the public settings are saved; main uses it to apply the
     *        Modbus address (was the call to Program.ApplyModbusAddress).
     */
    void setPublicSavedHandler(std::function<void()> handler);

private:
    /**
     * @brief httpd entry point for every GET and POST; calls handleGet or handlePost (was ServerCommandReceived).
     */
    static esp_err_t onRequest(httpd_req* req);
    /**
     * @brief Answer a GET: logo, captive-portal probes, the pages and the trace (was HandleGet).
     */
    esp_err_t handleGet(httpd_req* req, const std::string& urlPath);
    /**
     * @brief Answer a form POST: save the settings, send the page again, reboot if needed (was HandlePost).
     */
    esp_err_t handlePost(httpd_req* req, const std::string& urlPath);

    /**
     * @brief Add the headers that keep browsers from caching the answer (was SetNoCacheHeaders).
     */
    static void setNoCacheHeaders(httpd_req* req);
    /**
     * @brief Send a 302 redirect to location (was Redirect).
     */
    static esp_err_t redirect(httpd_req* req, const char* location);
    /**
     * @brief Send an empty answer with this status, e.g. "404 Not Found" (new; was StatusCode + OutputAsStream "").
     */
    static esp_err_t sendEmpty(httpd_req* req, const char* status);
    /**
     * @brief Ask the browser for the admin login (was SendUnauthorizedResponse).
     */
    static esp_err_t sendUnauthorizedResponse(httpd_req* req);
    /**
     * @brief Send a page in chunks, so it is never in RAM as a whole (was SendPage and SendText).
     */
    static esp_err_t sendPage(httpd_req* req, const char* contentType, const PageWriter& writer);
    /**
     * @brief Read exactly contentLength bytes of request body (was ReadRequestBody).
     * @return false when the body is missing, too long or incomplete
     */
    static bool readRequestBody(httpd_req* req, size_t contentLength, std::string& body);

    /**
     * @brief Apply and save the public form, then call the public-saved handler (was ProcessPublicPost).
     * @return false when a number is invalid; nothing is applied or saved then
     */
    bool processPublicPost(const std::string& formData);
    /**
     * @brief Apply and save the advanced form, or one of its two reset buttons (was ProcessAdvancedPost).
     */
    void processAdvancedPost(const std::string& formData);
    /**
     * @brief Save the advanced settings and tell the gate to apply them (new; C# repeated this three times).
     */
    void saveAdvanced();
    /**
     * @brief Check the Basic auth header against the admin login (was IsAuthenticated).
     */
    static bool isAuthenticated(httpd_req* req);

    // Settings, storage and gate (owned by main.cpp)
    PublicSettings& pub;
    AdvancedSettings& adv;
    const TeachSettings& teach;
    ConfigStorage& store;
    GateController& gate;

    std::string softApIp;                 ///< redirect target (was WirelessAP.SoftApIP)
    void* server = nullptr;               ///< httpd_handle_t, null while stopped (was _server)
    bool rebootAfterResponse = false;     ///< the encoder mode changed: reboot after the answer (was _rebootAfterResponse)
    std::function<void()> onPublicSaved;  ///< see setPublicSavedHandler()
};

#endif // WEB_WEB_SERVER_H
