#include "modules/SetupPortal.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <string.h>

#include "config.h"
#include "modules/SetupPortalPage.h"

namespace {
constexpr size_t kMinPasswordLength = 8;  // WPA2

WebServer server(WEB_PORT);
DNSServer dns;
PortalStationState stationState = PortalStationState::Idle;
WifiCredential submitted = {};
bool hasSubmitted = false;

const char* stationStateName(PortalStationState state) {
    switch (state) {
        case PortalStationState::Idle: return "idle";
        case PortalStationState::Connecting: return "connecting";
        case PortalStationState::Connected: return "connected";
        case PortalStationState::Failed: return "failed";
    }
    return "idle";
}

void sendJson(int code, const JsonDocument& doc) {
    String body;
    serializeJson(doc, body);
    server.send(code, "application/json", body);
}

void sendError(const char* message) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = message;
    sendJson(400, doc);
}

void handleScan() {
    if (server.hasArg("refresh") && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
        WiFi.scanDelete();
        WiFi.scanNetworks(true);
    }
    JsonDocument doc;
    const int16_t count = WiFi.scanComplete();
    doc["scanning"] = count == WIFI_SCAN_RUNNING;
    JsonArray networks = doc["networks"].to<JsonArray>();
    if (count == WIFI_SCAN_FAILED) {
        WiFi.scanNetworks(true);
        doc["scanning"] = true;
    }
    // Kết quả đã xếp theo RSSI giảm dần; bỏ mạng ẩn và tên trùng (nhiều điểm phát)
    for (int16_t i = 0; i < count && networks.size() < WIFI_SCAN_MAX_RESULTS; i++) {
        const String ssid = WiFi.SSID(i);
        bool duplicate = ssid.isEmpty();
        for (JsonVariantConst item : networks) {
            duplicate = duplicate || ssid == item["ssid"].as<const char*>();
        }
        if (duplicate) {
            continue;
        }
        JsonObject network = networks.add<JsonObject>();
        network["ssid"] = ssid;
        network["rssi"] = WiFi.RSSI(i);
        network["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    sendJson(200, doc);
}

void handleSave() {
    const String ssid = server.arg("ssid");
    const String password = server.arg("password");
    if (ssid.isEmpty() || ssid.length() >= sizeof(submitted.ssid)) {
        sendError("Tên WiFi không hợp lệ");
        return;
    }
    if (password.length() >= sizeof(submitted.password) ||
        (!password.isEmpty() && password.length() < kMinPasswordLength)) {
        sendError("Mật khẩu WiFi không hợp lệ");
        return;
    }
    strncpy(submitted.ssid, ssid.c_str(), sizeof(submitted.ssid) - 1);
    strncpy(submitted.password, password.c_str(), sizeof(submitted.password) - 1);
    hasSubmitted = true;
    stationState = PortalStationState::Connecting;
    JsonDocument doc;
    doc["ok"] = true;
    sendJson(200, doc);
}

void handleStatus() {
    JsonDocument doc;
    doc["state"] = stationStateName(stationState);
    if (stationState == PortalStationState::Connected) {
        doc["ip"] = WiFi.localIP().toString();
    }
    sendJson(200, doc);
}

// Mọi đường dẫn lạ (trang kiểm tra Internet của Windows/Android/iOS) -> chuyển về trang cài đặt
void handleNotFound() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
}
}  // namespace

void SetupPortal::begin() {
    if (active_) {
        return;
    }
    stationState = PortalStationState::Idle;
    hasSubmitted = false;
    dns.setErrorReplyCode(DNSReplyCode::NoError);
    dns.start(DNS_PORT, "*", WiFi.softAPIP());
    static bool routesAdded = false;  // WebServer không gỡ route được: chỉ đăng ký một lần
    if (!routesAdded) {
        server.on("/", HTTP_GET, [] { server.send(200, "text/html; charset=utf-8", kSetupPortalPage); });
        server.on("/scan", HTTP_GET, handleScan);
        server.on("/save", HTTP_POST, handleSave);
        server.on("/status", HTTP_GET, handleStatus);
        server.onNotFound(handleNotFound);
        routesAdded = true;
    }
    server.begin();
    WiFi.scanNetworks(true);
    active_ = true;
    log_i("Trang cai WiFi: http://%s", WiFi.softAPIP().toString().c_str());
}

void SetupPortal::end() {
    if (!active_) {
        return;
    }
    server.stop();
    dns.stop();
    WiFi.scanDelete();
    memset(&submitted, 0, sizeof(submitted));
    hasSubmitted = false;
    active_ = false;
}

bool SetupPortal::update(WifiCredential& out) {
    if (!active_) {
        return false;
    }
    dns.processNextRequest();
    server.handleClient();
    if (!hasSubmitted) {
        return false;
    }
    out = submitted;
    memset(&submitted, 0, sizeof(submitted));
    hasSubmitted = false;
    return true;
}

void SetupPortal::setStationState(PortalStationState state) { stationState = state; }
