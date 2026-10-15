#pragma once
#include <stdint.h>

#include "ConnectionState.h"

struct DisplayStatus {
    ConnectionState connection;
    uint8_t batteryPercent;
    bool charging;
    const char* setupSsid;      // WiFi riêng của mạch, nullptr nếu không ở chế độ cài đặt
    const char* setupPassword;
    const char* pairingCode;    // nullptr nếu không ghép đôi
    const char* message;      // Dòng phụ, nullptr nếu không có
};

// OLED SSD1306: hiện trạng thái, pin, mã ghép đôi. Tự tắt sau OLED_TIMEOUT_MS.
// Bus I2C được khởi tạo trong boardBegin().
class DisplayOled {
public:
    bool begin();
    void wake(uint32_t nowMs);
    void sleep();
    void update(uint32_t nowMs);
    void show(const DisplayStatus& status);
    bool isAwake() const { return awake_; }
    // Không cho bật lại (pin yếu <= 10%)
    void setDisabled(bool disabled);

private:
    bool ready_ = false;
    bool awake_ = false;
    bool disabled_ = false;
    uint32_t wakeMs_ = 0;
};
