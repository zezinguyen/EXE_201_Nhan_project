#include "Provisioning.h"
#include "AudioPlayer.h"
#include "DisplayOled.h"
#include "StatusLed.h"
#include <WiFi.h>
#include <esp_system.h>

void Provisioning::begin() {
    prefs.begin("smarthome", false);

    paired = prefs.getBool("paired", true); // Mặc định là paired với board_living_01 để dễ dùng thử
    boardId = prefs.getString("boardId", "board_living_01");
    boardToken = prefs.getString("boardToken", "");
    homeId = prefs.getString("homeId", "00000000-0000-0000-0000-000000000001");
    wifiSsid = prefs.getString("wifiSsid", "");
    wifiPassword = prefs.getString("wifiPass", "");
    supabaseUrl = prefs.getString("supaUrl", "");
    supabaseAnonKey = prefs.getString("supaAnon", "");

    Serial.printf("[PROV] Khởi động: Paired=%d, BoardId=%s, HomeId=%s\n", 
        paired, boardId.c_str(), homeId.c_str());

    // Nếu chưa có WiFi, tự động vào chế độ thiết lập
    if (wifiSsid.isEmpty()) {
        startPairingMode();
    }
}

String Provisioning::generateRandomString(size_t len) {
    const char charset[] = "0123456789ABCDEF";
    String result = "";
    for (size_t i = 0; i < len; i++) {
        result += charset[esp_random() % (sizeof(charset) - 1)];
    }
    return result;
}

String Provisioning::generateSixDigitCode() {
    uint32_t num = esp_random() % 900000 + 100000;
    return String(num);
}

void Provisioning::startPairingMode() {
    if (inPairingMode) return;
    inPairingMode = true;
    pairingStartTime = millis();

    // Sinh SSID, mật khẩu ngẫu nhiên và mã 6 số (Spec 10.2)
    apSsid = "ESP32_IR_" + generateRandomString(4);
    apPassword = generateRandomString(8);
    pairingCode = generateSixDigitCode();

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSsid.c_str(), apPassword.c_str());

    Serial.println("\n--- [CHẾ ĐỘ THIẾT LẬP WIFI & GHÉP ĐÔI (10 PHÚT)] ---");
    Serial.printf("SSID AP: %s\n", apSsid.c_str());
    Serial.printf("Mật khẩu AP: %s\n", apPassword.c_str());
    Serial.printf("Mã ghép đôi 6 số: %s\n", pairingCode.c_str());
    Serial.printf("IP AP: %s\n", WiFi.softAPIP().toString().c_str());

    StatusLed::instance().setPattern(LED_SETUP_PAIRING);
    AudioPlayer::instance().playTrack(TRACK_ENTER_SETUP);
    DisplayOled::instance().showPairingInfo(apSsid, apPassword, pairingCode);

    setupApWebServer();
}

void Provisioning::setupApWebServer() {
    server.on("/", HTTP_GET, [this]() {
        String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>SmartHome ESP32 Setup</title><style>body{font-family:sans-serif;padding:20px;max-width:400px;margin:auto;}input{width:100%;padding:10px;margin:8px 0;box-sizing:border-box;}button{width:100%;padding:12px;background:#0066cc;color:#fff;border:none;border-radius:6px;font-size:16px;}</style></head><body>";
        html += "<h2>Cài đặt ESP32 SmartHome</h2>";
        html += "<p>Mã ghép đôi trên OLED: <b>" + pairingCode + "</b></p>";
        html += "<form method='POST' action='/save'>";
        html += "<label>WiFi Nhà bạn:</label><input name='ssid' required value='" + wifiSsid + "'>";
        html += "<label>Mật khẩu WiFi:</label><input name='pass' type='password'>";
        html += "<label>Supabase URL:</label><input name='supa_url' placeholder='https://xxx.supabase.co' value='" + supabaseUrl + "'>";
        html += "<label>Supabase Anon Key:</label><input name='supa_anon' placeholder='eyJhbG...' value='" + supabaseAnonKey + "'>";
        html += "<button type='submit'>Lưu và Kết nối</button></form></body></html>";
        server.send(200, "text/html", html);
    });

    server.on("/save", HTTP_POST, [this]() {
        if (server.hasArg("ssid")) {
            saveWifi(server.arg("ssid"), server.arg("pass"));
        }
        if (server.hasArg("supa_url") && server.hasArg("supa_anon")) {
            saveSupabaseConfig(server.arg("supa_url"), server.arg("supa_anon"));
        }
        server.send(200, "text/html", "<h3>Đã lưu! ESP32 đang kết nối WiFi...</h3>");
        delay(1000);
        stopPairingMode();
        ESP.restart();
    });

    server.begin();
}

void Provisioning::stopPairingMode() {
    if (!inPairingMode) return;
    inPairingMode = false;
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);

    DisplayOled::instance().showStatus("Dang ket noi...", "WiFi");
    StatusLed::instance().setPattern(LED_WIFI_NO_INTERNET);
}

void Provisioning::update() {
    if (inPairingMode) {
        server.handleClient();
        // Hết hạn sau 10 phút (Spec 10.2)
        if (millis() - pairingStartTime >= PAIRING_MODE_TIMEOUT_MS) {
            Serial.println("[PROV] Hết hạn 10 phút chế độ ghép đôi, tắt AP.");
            stopPairingMode();
        }
    }
}

void Provisioning::saveCredentials(const String& bId, const String& bToken, const String& hId) {
    boardId = bId;
    boardToken = bToken;
    homeId = hId;
    paired = true;

    prefs.putString("boardId", boardId);
    prefs.putString("boardToken", boardToken);
    prefs.putString("homeId", homeId);
    prefs.putBool("paired", true);
}

void Provisioning::saveWifi(const String& ssid, const String& pass) {
    wifiSsid = ssid;
    wifiPassword = pass;
    prefs.putString("wifiSsid", wifiSsid);
    prefs.putString("wifiPass", wifiPassword);
}

void Provisioning::saveSupabaseConfig(const String& url, const String& anon) {
    supabaseUrl = url;
    supabaseAnonKey = anon;
    prefs.putString("supaUrl", supabaseUrl);
    prefs.putString("supaAnon", supabaseAnonKey);
}

void Provisioning::factoryReset() {
    Serial.println("[PROV] ĐANG KHÔI PHỤC CÀI ĐẶT GỐC...");
    AudioPlayer::instance().playTrack(TRACK_FACTORY_RESET);
    DisplayOled::instance().showTemporaryMessage("KHOI PHUC GOC", "Dang khoi dong lai...", 3000);
    delay(2000);

    prefs.clear();
    WiFi.disconnect(true, true);
    delay(500);
    ESP.restart();
}

