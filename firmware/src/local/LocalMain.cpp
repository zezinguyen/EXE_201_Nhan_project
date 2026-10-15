// BẢN THỬ không cần server (env esp32dev_local) – thay cho src/main.cpp.
//   1. Chưa có WiFi: mạch bật WiFi riêng, OLED hiện tên + mật khẩu. Laptop nối vào, trang cài WiFi
//      tự mở (hoặc vào http://192.168.4.1), chọn WiFi nhà.
//   2. Vào WiFi nhà: OLED hiện IP. Laptop cùng WiFi mở http://smarthome.local (hoặc http://<IP>)
//      để điều khiển điều hòa Funiki, học phím, dò giao thức.
// Nút: bấm ngắn = sáng OLED, giữ 5s = học phím (xem trên web), 10s = cài lại WiFi, 20s = xoá WiFi.

#ifndef PIO_UNIT_TESTING

#include <Arduino.h>

#include "LocalWebServer.h"
#include "app/CommandHandler.h"
#include "app/Feedback.h"
#include "app/Outbox.h"
#include "config.h"
#include "modules/AudioPlayer.h"
#include "modules/Board.h"
#include "modules/DeviceStore.h"
#include "modules/DisplayOled.h"
#include "modules/ErrorReporter.h"
#include "modules/IrLearner.h"
#include "modules/IrSender.h"
#include "modules/PowerManager.h"
#include "modules/RealtimeClient.h"
#include "modules/SecureStore.h"
#include "modules/StatusLed.h"
#include "modules/SupabaseRest.h"
#include "modules/TimeSync.h"
#include "modules/UserButton.h"
#include "modules/WifiManager.h"

namespace {
constexpr uint32_t kMessageShowMs = 5000;

SecureStore secureStore;
PowerManager power;
StatusLed statusLed;
DisplayOled display;
UserButton button;
AudioPlayer audio;
IrSender irSender;
IrLearner irLearner;
DeviceStore deviceStore;
ErrorReporter errorReporter;
WifiManager wifi;
TimeSync timeSync;
// Không kết nối Supabase: chỉ để dựng CommandHandler (ACK gửi đi bị bỏ qua vì chưa vào kênh)
RealtimeClient realtime;
SupabaseRest rest;
Outbox outbox(rest, errorReporter);
CommandHandler commandHandler(deviceStore, irSender, irLearner, errorReporter, timeSync, realtime, outbox);
Feedback feedback(audio);
LocalWebServer web(deviceStore, commandHandler, irSender, irLearner, power, wifi, feedback);

ConnectionState connection = ConnectionState::NoWifi;
uint32_t lastDisplayMs = 0;
const char* message = nullptr;  // Thông báo tạm trên OLED
uint32_t messageUntilMs = 0;
char webAddress[24] = {};

void showMessage(const char* text, uint32_t nowMs) {
    message = text;
    messageUntilMs = nowMs + kMessageShowMs;
    display.wake(nowMs);
}

void enterSetupMode(uint32_t nowMs) {
    wifi.startSetupPortal(nowMs);
    feedback.onSetupMode();
}

void onButtonAction(ButtonAction action) {
    const uint32_t nowMs = millis();
    display.wake(nowMs);
    switch (action) {
        case ButtonAction::None:
        case ButtonAction::ShowStatus:
            break;
        case ButtonAction::LearnCommand:
            web.startLearning(nowMs);
            break;
        case ButtonAction::SetupMode:
            enterSetupMode(nowMs);
            break;
        case ButtonAction::FactoryReset:
            log_w("Xoa WiFi da luu");
            secureStore.clearAll();
            feedback.onFactoryReset();
            for (uint32_t start = millis(); millis() - start < AUDIO_MAX_PLAY_MS && audio.isBusy();) {
                audio.update(millis());
                delay(LOOP_IDLE_MS);
            }
            boardRestart();
            break;
    }
}

void onLearnEvent(LearnEvent event, uint32_t nowMs) {
    switch (event) {
        case LearnEvent::None:
            return;
        case LearnEvent::FirstCaptured:
            showMessage("Bam lai lan 2", nowMs);
            break;
        case LearnEvent::Mismatch:
            showMessage("Khong khop, bam lai", nowMs);
            break;
        case LearnEvent::Success:
            showMessage("Da hoc, dat ten tren web", nowMs);
            break;
        case LearnEvent::Failed:
        case LearnEvent::Timeout:
            showMessage("Hoc lenh that bai", nowMs);
            break;
    }
    web.onLearnEvent(event);
    feedback.onLearnEvent(event);
}

// Web điều khiển dùng chung cổng 80 với trang cài WiFi: chỉ chạy khi WiFi cài đặt đã tắt
void updateWeb() {
    const bool wanted = wifi.isConnected() && !wifi.isSetupPortalActive();
    if (wanted && !web.isRunning()) {
        web.begin();
        char ip[16];
        wifi.localIp(ip, sizeof(ip));
        snprintf(webAddress, sizeof(webAddress), "Web:%s", ip);
        display.wake(millis());
    } else if (!wanted && web.isRunning()) {
        web.end();
        webAddress[0] = '\0';
    }
    web.update();
}

const char* currentMessage(uint32_t nowMs) {
    static char holdHint[32];
    const ButtonAction pending = button.actionIfReleased(nowMs);
    if (pending != ButtonAction::None && pending != ButtonAction::ShowStatus) {
        snprintf(holdHint, sizeof(holdHint), "Tha: %s", buttonActionText(pending));
        return holdHint;
    }
    if (message != nullptr && static_cast<int32_t>(nowMs - messageUntilMs) < 0) {
        return message;
    }
    if (commandHandler.isLearning()) {
        return "Bam phim tren remote";
    }
    return webAddress[0] != '\0' ? webAddress : nullptr;
}

LedPattern currentLedPattern() {
    if (connection == ConnectionState::SetupMode) {
        return LedPattern::SetupMode;
    }
    if (commandHandler.isLearning()) {
        return LedPattern::Learning;
    }
    if (connection == ConnectionState::NoWifi) {
        return LedPattern::NoWifi;
    }
    const bool lowBattery = power.source() == PowerSource::Battery && power.batteryLevel() >= BatteryLevel::Low;
    return lowBattery ? LedPattern::LowBattery : LedPattern::Online;
}

void updateIndicators(uint32_t nowMs) {
    if (wifi.isSetupPortalActive()) {
        connection = ConnectionState::SetupMode;
    } else if (!wifi.isConnected()) {
        connection = ConnectionState::NoWifi;
    } else {
        connection = ConnectionState::Online;  // Bản thử: có WiFi nhà là dùng được
    }
    statusLed.setPattern(currentLedPattern());
    statusLed.update(nowMs);

    if (button.isPressed() || connection == ConnectionState::SetupMode || commandHandler.isLearning()) {
        display.wake(nowMs);
    }
    display.update(nowMs);
    if (display.isAwake() && nowMs - lastDisplayMs >= DISPLAY_REFRESH_MS) {
        lastDisplayMs = nowMs;
        DisplayStatus status = {};
        status.connection = connection;
        status.batteryPercent = power.batteryPercent();
        status.charging = power.isCharging();
        status.setupSsid = wifi.setupSsid();
        status.setupPassword = wifi.setupPassword();
        status.pairingCode = nullptr;
        status.message = currentMessage(nowMs);
        display.show(status);
    }
}
}  // namespace

void setup() {
    boardBegin();
    log_i("SmartHome IR firmware %s - BAN THU (web cuc bo)", FIRMWARE_VERSION);

    secureStore.begin();
    power.begin();
    statusLed.begin();
    button.begin(onButtonAction);
    audio.begin();
    irSender.begin();
    irLearner.begin();
    if (display.begin()) {
        display.wake(millis());
    }
    if (!deviceStore.begin() || !deviceStore.load()) {
        log_w("Chua co profile, tao moi");
    }
    web.ensureDefaultProfile();
    wifi.begin(secureStore);
    timeSync.begin();

    if (secureStore.wifiCount() == 0) {
        enterSetupMode(millis());
    }
}

void loop() {
    const uint32_t nowMs = millis();

    power.update(nowMs);
    wifi.update(nowMs);
    updateWeb();
    button.update(nowMs);
    onLearnEvent(commandHandler.update(nowMs), nowMs);
    audio.update(nowMs);
    updateIndicators(nowMs);

    delay(LOOP_IDLE_MS);
}

#endif  // PIO_UNIT_TESTING
