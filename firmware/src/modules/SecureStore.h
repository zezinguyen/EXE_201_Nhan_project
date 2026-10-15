#pragma once
#include <stddef.h>
#include <stdint.h>

#include "config.h"

struct WifiCredential {
    char ssid[33];
    char password[65];
};

// Lưu WiFi (<= WIFI_MAX_NETWORKS), token, id mạch và id nhà vào bộ nhớ không mất khi tắt điện.
// KHÔNG in mật khẩu/token ra log. Triển khai riêng theo MCU: src/platform/<mcu>/SecureStore.cpp
class SecureStore {
public:
    bool begin();

    uint8_t wifiCount() const;
    bool loadWifi(uint8_t index, WifiCredential& out) const;
    // Thêm mạng mới lên đầu danh sách, đẩy mạng cũ nhất ra nếu đầy.
    bool addWifi(const WifiCredential& credential);

    bool hasBoardToken() const;
    bool loadBoardToken(char* out, size_t outSize) const;
    bool saveBoardToken(const char* token);

    bool loadBoardId(char* out, size_t outSize) const;
    bool saveBoardId(const char* boardId);

    bool loadHomeId(char* out, size_t outSize) const;
    bool saveHomeId(const char* homeId);

    // Khôi phục gốc: xoá WiFi, token, id
    void clearAll();
};
