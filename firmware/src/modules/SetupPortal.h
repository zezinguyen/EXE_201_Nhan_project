#pragma once
#include <stdint.h>

#include "SecureStore.h"

enum class PortalStationState : uint8_t {
    Idle,        // Chưa nhập WiFi
    Connecting,  // Vừa nhập, đang thử nối WiFi nhà
    Connected,   // Đã vào WiFi nhà (trang hiện IP)
    Failed,      // Hết thời gian chờ: sai mật khẩu / mạng ở xa
};

// Trang cài WiFi trên WiFi riêng của mạch (captive portal): DNS trả mọi tên miền về mạch nên
// điện thoại/laptop tự mở trang; chọn WiFi nhà + nhập mật khẩu (spec 9.2 D, 10.2 bước 3).
// Chỉ để nhập WiFi, KHÔNG điều khiển thiết bị. WifiManager bật/tắt trang cùng WiFi riêng.
// Triển khai riêng theo MCU: src/platform/<mcu>/SetupPortal.cpp
class SetupPortal {
public:
    // Gọi sau khi WiFi riêng đã bật
    void begin();
    void end();
    // true khi người dùng vừa gửi WiFi mới (ghi vào out)
    bool update(WifiCredential& out);
    // reason: mã lý do ngắt WiFi của MCU khi Failed (0 = không rõ) -> trang hiện gợi ý sửa
    void setStationState(PortalStationState state, uint8_t reason = 0);
    bool isActive() const { return active_; }

private:
    bool active_ = false;
};
