#pragma once
#include <ArduinoJson.h>
#include <stdint.h>

#include "app/CommandHandler.h"
#include "app/Feedback.h"
#include "modules/DeviceStore.h"
#include "modules/IrLearner.h"
#include "modules/IrSender.h"
#include "modules/PowerManager.h"
#include "modules/WifiManager.h"

// Web điều khiển trong mạng nhà cho BẢN THỬ khi chưa có server (env esp32dev_local).
// Mở http://smarthome.local (mDNS) hoặc http://<IP trên OLED>. Lệnh đi qua CommandHandler::runAction
// như lệnh thật; phím học được và giao thức điều hòa lưu thẳng vào profile trên mạch.
// KHÔNG có đăng nhập: ai cùng WiFi cũng điều khiển được -> không đưa vào bản phát hành.
// Chỉ chạy trên ESP32 (WebServer, ESPmDNS của Arduino-ESP32).
class LocalWebServer {
public:
    LocalWebServer(DeviceStore& store, CommandHandler& commands, IrSender& sender, IrLearner& learner,
                   PowerManager& power, WifiManager& wifi, Feedback& feedback);

    // Thêm điều hòa Funiki vào profile nếu chưa có
    void ensureDefaultProfile();
    // Gọi khi đã vào WiFi nhà và WiFi cài đặt đã tắt (cùng cổng 80 với trang cài WiFi)
    void begin();
    void end();
    bool isRunning() const { return running_; }
    void update();

    // Học/dò mã: từ web hoặc nút giữ 5s. Kết quả hiện trên web, lưu thành phím khi người dùng đặt tên.
    void startLearning(uint32_t nowMs);
    void onLearnEvent(LearnEvent event);

private:
    void handleState();
    void handleProfile();
    void handleProtocols();
    void handleCommand();
    void handleAcTry();
    void handleAcProtocol();
    void handleLearnStart();
    void handleLearnCancel();
    void handleLearnSave();
    void handleButtonDelete();

    bool readBody(JsonDocument& out);
    void sendJson(int code, const JsonDocument& doc);
    void sendResult(const char* error);
    bool saveProfile(const JsonDocument& doc);

    DeviceStore& store_;
    CommandHandler& commands_;
    IrSender& sender_;
    IrLearner& learner_;
    PowerManager& power_;
    WifiManager& wifi_;
    Feedback& feedback_;
    bool running_ = false;
    bool routesAdded_ = false;
    const char* learnStatus_ = "idle";
    char detected_[24] = {};
    JsonDocument lastCapture_;  // IrCode của lần học thành công gần nhất, chờ đặt tên
};
