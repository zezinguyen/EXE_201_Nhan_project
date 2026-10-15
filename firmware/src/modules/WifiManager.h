#pragma once
#include <Backoff.h>
#include <stddef.h>
#include <stdint.h>

#include "SecureStore.h"
#include "SetupPortal.h"
#include "config.h"

// Nối WiFi theo danh sách đã lưu, tự nối lại với khoảng chờ tăng dần,
// mất WiFi quá lâu thì bật WiFi riêng + trang cài WiFi (spec 9.2 B, D).
// WiFi riêng tự tắt khi đã vào WiFi nhà: sau WIFI_SETUP_LINGER_MS nếu vừa nhập WiFi mới
// (để trang kịp hiện IP), hoặc sau WIFI_SETUP_IDLE_MS nếu không ai nhập.
// Triển khai riêng theo MCU: src/platform/<mcu>/WifiManager.cpp
class WifiManager {
public:
    WifiManager();

    void begin(SecureStore& store);
    void update(uint32_t nowMs);

    bool isConnected() const;
    int8_t rssi() const;
    // IP trong mạng nhà, "" khi chưa kết nối
    void localIp(char* out, size_t outSize) const;

    // WiFi riêng + trang cài đặt WiFi (chỉ để nhập WiFi, không dùng để điều khiển)
    void startSetupPortal(uint32_t nowMs);
    void stopSetupPortal();
    bool isSetupPortalActive() const { return portalActive_; }
    // Tên/mật khẩu WiFi riêng để hiện trên OLED (nullptr khi không ở chế độ cài đặt)
    const char* setupSsid() const { return portalActive_ ? setupSsid_ : nullptr; }
    const char* setupPassword() const { return portalActive_ ? setupPassword_ : nullptr; }

private:
    void tryNextNetwork(uint32_t nowMs);
    void updatePortal(uint32_t nowMs);

    SecureStore* store_ = nullptr;
    SetupPortal portal_;
    bool portalSubmitted_ = false;  // Đã nhập WiFi qua trang trong lần bật này
    bool portalAttempt_ = false;    // Đang thử mạng vừa nhập -> báo kết quả lên trang
    uint32_t portalStartMs_ = 0;
    uint32_t connectedAtMs_ = 0;
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
