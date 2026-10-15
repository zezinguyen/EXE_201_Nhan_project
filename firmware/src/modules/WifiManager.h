#pragma once
#include <Backoff.h>
#include <stdint.h>

#include "SecureStore.h"
#include "config.h"

// Nối WiFi theo danh sách đã lưu, tự nối lại với khoảng chờ tăng dần,
// mất WiFi quá lâu thì bật WiFi riêng để cài đặt (spec 9.2 B, D).
// Triển khai riêng theo MCU: src/platform/<mcu>/WifiManager.cpp
class WifiManager {
public:
    WifiManager();

    void begin(SecureStore& store);
    void update(uint32_t nowMs);

    bool isConnected() const;
    int8_t rssi() const;

    // WiFi riêng + trang cài đặt WiFi (chỉ để nhập WiFi, không dùng để điều khiển)
    void startSetupPortal(uint32_t nowMs);
    void stopSetupPortal();
    bool isSetupPortalActive() const { return portalActive_; }
    // Tên/mật khẩu WiFi riêng để hiện trên OLED (nullptr khi không ở chế độ cài đặt)
    const char* setupSsid() const { return portalActive_ ? setupSsid_ : nullptr; }
    const char* setupPassword() const { return portalActive_ ? setupPassword_ : nullptr; }

private:
    void tryNextNetwork(uint32_t nowMs);

    SecureStore* store_ = nullptr;
    Backoff backoff_;
    uint8_t networkIndex_ = 0;
    bool connecting_ = false;
    bool portalActive_ = false;
    uint32_t attemptStartMs_ = 0;
    uint32_t nextAttemptMs_ = 0;
    uint32_t disconnectedSinceMs_ = 0;
    char setupSsid_[32] = {};
    char setupPassword_[WIFI_SETUP_AP_PASSWORD_LENGTH + 1] = {};
};
