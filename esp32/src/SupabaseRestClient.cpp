#include "SupabaseRestClient.h"
#include "Provisioning.h"
#include "PowerManager.h"
#include "DeviceStore.h"
#include <WiFi.h>
#include <time.h>
#include <ArduinoJson.h>

void SupabaseRestClient::begin() {
    lastStatusReportTime = millis();
}

bool SupabaseRestClient::postRpc(const String& rpcName, const String& jsonBody) {
    if (WiFi.status() != WL_CONNECTED) return false;

    String baseUrl = Provisioning::instance().getSupabaseUrl();
    String anonKey = Provisioning::instance().getSupabaseAnonKey();
    String token = Provisioning::instance().getBoardToken();

    if (baseUrl.isEmpty() || anonKey.isEmpty()) {
        Serial.printf("[REST] Supabase URL hoặc Anon Key chưa cấu hình (RPC: %s)\n", rpcName.c_str());
        return false;
    }

    String url = baseUrl + "/rest/v1/rpc/" + rpcName;
    WiFiClientSecure client;
    client.setInsecure(); // Giai đoạn prototype, nạp CA ở bản đại trà

    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", anonKey);
    http.addHeader("Authorization", "Bearer " + (token.isEmpty() ? anonKey : token));
    http.addHeader("Prefer", "return=minimal");
    http.setTimeout(4000);

    int httpCode = http.POST(jsonBody);
    bool ok = (httpCode >= 200 && httpCode < 300);
    if (!ok) {
        Serial.printf("[REST] Gọi RPC %s lỗi HTTP %d: %s\n", rpcName.c_str(), httpCode, http.getString().c_str());
    }
    http.end();
    return ok;
}

bool SupabaseRestClient::postRest(const String& table, const String& jsonBody) {
    if (WiFi.status() != WL_CONNECTED) return false;

    String baseUrl = Provisioning::instance().getSupabaseUrl();
    String anonKey = Provisioning::instance().getSupabaseAnonKey();
    String token = Provisioning::instance().getBoardToken();

    if (baseUrl.isEmpty() || anonKey.isEmpty()) return false;

    String url = baseUrl + "/rest/v1/" + table;
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", anonKey);
    http.addHeader("Authorization", "Bearer " + (token.isEmpty() ? anonKey : token));
    http.addHeader("Prefer", "return=minimal");
    http.setTimeout(4000);

    int httpCode = http.POST(jsonBody);
    bool ok = (httpCode == 201 || (httpCode >= 200 && httpCode < 300));
    http.end();
    return ok;
}

bool SupabaseRestClient::sendAck(const String& commandId, bool ok, const String& error, bool duplicate) {
    JsonDocument doc;
    doc["commandId"] = commandId;
    doc["ok"] = ok;
    if (!ok && !error.isEmpty()) {
        doc["error"] = error;
    }
    if (duplicate) {
        doc["duplicate"] = true;
    }

    String body;
    serializeJson(doc, body);
    Serial.printf("[REST] Gửi ACK: %s\n", body.c_str());
    return postRpc("board_ack", body);
}

bool SupabaseRestClient::sendLearnEvent(const String& event, const String& requestId, const String& status, const IrCode* ir) {
    JsonDocument doc;
    doc["event"] = event;
    doc["requestId"] = requestId;
    doc["status"] = status;
    if (ir != nullptr) {
        JsonObject irObj = doc["ir"].to<JsonObject>();
        ir->toJson(irObj);
    }

    String body;
    serializeJson(doc, body);
    Serial.printf("[REST] Gửi Learn Event: %s\n", body.c_str());
    return postRpc("board_learn_event", body);
}

bool SupabaseRestClient::sendStatusReport() {
    JsonDocument doc;
    doc["battery"] = PowerManager::instance().getBatteryPercent();
    doc["charging"] = PowerManager::instance().isCharging();
    doc["usb"] = PowerManager::instance().isUsbConnected();
    doc["rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -100;
    doc["firmware"] = FIRMWARE_VERSION;
    doc["profileVersion"] = DeviceStore::instance().getProfileVersion();
    doc["pendingLearnedCodes"] = (int)pendingLearnedCodes.size();

    String body;
    serializeJson(doc, body);
    Serial.printf("[REST] Gửi board_status: %s\n", body.c_str());
    return postRpc("board_status", body);
}

void SupabaseRestClient::queueLearnedCode(const IrCode& code) {
    if (pendingLearnedCodes.size() >= MAX_OFFLINE_LEARNED_CODES) {
        pendingLearnedCodes.erase(pendingLearnedCodes.begin()); // Bỏ mã cũ nhất nếu đầy
    }
    pendingLearnedCodes.push_back(code);
    drainQueues();
}

void SupabaseRestClient::queueErrorReport(const String& code, const String& deviceId) {
    if (pendingErrorReports.size() >= MAX_OFFLINE_ERROR_REPORTS) {
        pendingErrorReports.erase(pendingErrorReports.begin());
    }
    QueuedErrorReport r;
    r.code = code;
    r.deviceId = deviceId;
    r.occurredEpoch = (uint32_t)time(nullptr);
    pendingErrorReports.push_back(r);
    drainQueues();
}

void SupabaseRestClient::drainQueues() {
    if (WiFi.status() != WL_CONNECTED) return;

    // Gửi bù các mã học từ nút bấm
    while (!pendingLearnedCodes.empty()) {
        const IrCode& ir = pendingLearnedCodes.front();
        JsonDocument doc;
        doc["home_id"] = Provisioning::instance().getHomeId();
        doc["board_id"] = Provisioning::instance().getBoardId();
        time_t now = time(nullptr);
        if (now > 1700000000) {
            doc["learned_at_epoch"] = (uint64_t)now;
        }
        JsonObject irObj = doc["ir"].to<JsonObject>();
        ir.toJson(irObj);

        String body;
        serializeJson(doc, body);
        if (postRest("learned_codes", body)) {
            Serial.println("[REST] Gửi bù learned_codes thành công!");
            pendingLearnedCodes.erase(pendingLearnedCodes.begin());
        } else {
            break; // Thử lại sau nếu lỗi mạng
        }
    }

    // Gửi bù các log báo cáo lỗi
    while (!pendingErrorReports.empty()) {
        const QueuedErrorReport& rep = pendingErrorReports.front();
        JsonDocument doc;
        doc["home_id"] = Provisioning::instance().getHomeId();
        doc["board_id"] = Provisioning::instance().getBoardId();
        doc["code"] = rep.code;
        if (!rep.deviceId.isEmpty()) {
            doc["device_id"] = rep.deviceId;
        }
        doc["occurred_at_epoch"] = rep.occurredEpoch;
        doc["firmware"] = FIRMWARE_VERSION;

        String body;
        serializeJson(doc, body);
        if (postRest("error_reports", body)) {
            pendingErrorReports.erase(pendingErrorReports.begin());
        } else {
            break;
        }
    }
}

void SupabaseRestClient::update() {
    unsigned long now = millis();

    // Báo cáo trạng thái mạch định kỳ mỗi 5 phút (Contract 3.6)
    if (now - lastStatusReportTime >= STATUS_REPORT_INTERVAL_MS) {
        lastStatusReportTime = now;
        sendStatusReport();
    }

    // Thử gửi bù hàng đợi mỗi 30 giây (Contract 4)
    if (now - lastQueueDrainTime >= 30000) {
        lastQueueDrainTime = now;
        drainQueues();
    }
}

