#pragma once
#include <stdint.h>

// Trạng thái kết nối tổng hợp – dùng cho LED, OLED, Presence (spec 9.3)
enum class ConnectionState : uint8_t {
    NoWifi,
    NoInternet,  // Có WiFi nhưng chưa vào được Supabase
    Online,
    SetupMode,   // Đang bật WiFi riêng để cài đặt / ghép đôi
};

inline const char* connectionStateText(ConnectionState state) {
    switch (state) {
        case ConnectionState::NoWifi: return "Mat WiFi";
        case ConnectionState::NoInternet: return "Mat Internet";
        case ConnectionState::Online: return "Online";
        case ConnectionState::SetupMode: return "Cai dat";
    }
    return "";
}
