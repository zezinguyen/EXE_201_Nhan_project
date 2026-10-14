#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <esp_wifi.h>

#include "config.h"
#include "PowerManager.h"
#include "DisplayOled.h"
#include "StatusLed.h"
#include "AudioPlayer.h"
#include "ButtonHandler.h"
#include "DeviceStore.h"
#include "IrSender.h"
#include "IrLearner.h"
#include "Provisioning.h"
#include "SupabaseRestClient.h"
#include "RealtimeClient.h"

void connectWiFi() {
    String ssid = Provisioning::instance().getWifiSsid();
    String pass = Provisioning::instance().getWifiPassword();

    if (ssid.isEmpty()) {
        Serial.println("[WIFI] Chưa cấu hình WiFi. Vào chế độ cài đặt.");
        Provisioning::instance().startPairingMode();
        return;
    }

    Serial.printf("[WIFI] Đang kết nối tới %s...\n", ssid.c_str());
    DisplayOled::instance().showStatus("Dang ket noi...", ssid);
    StatusLed::instance().setPattern(LED_NO_WIFI);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(200);
        StatusLed::instance().update();
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] Đã kết nối WiFi! IP: %s, RSSI: %d dBm\n", 
            WiFi.localIP().toString().c_str(), WiFi.RSSI());

        // Bật chế độ Modem-Sleep để tiết kiệm pin (Spec 5.2 Giai đoạn 1)
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

        // Đồng bộ thời gian NTP (GMT+7)
        configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");

        StatusLed::instance().setPattern(LED_WIFI_NO_INTERNET);
        DisplayOled::instance().showStatus("Co WiFi", "Dang noi Supabase...");
    } else {
        Serial.println("[WIFI] Kết nối WiFi thất bại!");
        StatusLed::instance().setPattern(LED_NO_WIFI);
        DisplayOled::instance().showStatus("Mat WiFi", "Thu lai sau...");
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n========================================================");
    Serial.println("  SMARTHOME IR CONTROLLER FIRMWARE v0.1.0");
    Serial.println("  Tuan thu specs.md, contracts.md, production-notes.md");
    Serial.println("========================================================\n");

    // Khởi tạo các module ngoại vi phần cứng
    PowerManager::instance().begin();
    StatusLed::instance().begin();
    DisplayOled::instance().begin();
    ButtonHandler::instance().begin();
    AudioPlayer::instance().begin();
    DeviceStore::instance().begin();
    IrSender::instance().begin();
    IrLearner::instance().begin();
    Provisioning::instance().begin();
    SupabaseRestClient::instance().begin();
    RealtimeClient::instance().begin();

    // Kết nối mạng
    if (!Provisioning::instance().isInPairingMode()) {
        connectWiFi();
    }
}

void loop() {
    // 1. Quản lý sự kiện nút bấm đa năng (Spec 0.5)
    ButtonEvent btn = ButtonHandler::instance().update();
    switch (btn) {
        case BTN_SHORT_PRESS:
            PowerManager::instance().wakeScreen();
            break;

        case BTN_HOLD_5S_LEARN:
            Serial.println("[BTN] Nhận lệnh Giữ 5s -> Học lệnh từ nút bấm");
            IrLearner::instance().startLearning(LEARN_FROM_BUTTON);
            break;

        case BTN_HOLD_10S_PAIRING:
            Serial.println("[BTN] Nhận lệnh Giữ 10s -> Bật chế độ Cài đặt & Ghép đôi");
            Provisioning::instance().startPairingMode();
            break;

        case BTN_HOLD_20S_RESET:
            Serial.println("[BTN] Nhận lệnh Giữ 20s -> Khôi phục cài đặt gốc");
            Provisioning::instance().factoryReset();
            break;

        default:
            break;
    }

    // 2. Cập nhật các module
    PowerManager::instance().update();
    StatusLed::instance().update();
    DisplayOled::instance().update();
    IrLearner::instance().update();
    Provisioning::instance().update();
    RealtimeClient::instance().update();
    SupabaseRestClient::instance().update();

    // 3. Tự động kết nối lại WiFi nếu bị mất (Spec 9.2)
    static unsigned long lastWifiCheck = 0;
    if (millis() - lastWifiCheck > 10000) {
        lastWifiCheck = millis();
        if (!Provisioning::instance().isInPairingMode() && WiFi.status() != WL_CONNECTED) {
            connectWiFi();
        }
    }

    delay(10);
}

