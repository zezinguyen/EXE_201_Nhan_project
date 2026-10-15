#pragma once
#include <ButtonDetector.h>
#include <stdint.h>

enum class ButtonAction : uint8_t {
    None,
    ShowStatus,    // Bấm ngắn: sáng OLED
    LearnCommand,  // Giữ 5s: học một lệnh IR, gửi lên làm "mã chờ gán"
    SetupMode,     // Giữ 10s: WiFi riêng + mã ghép đôi
    FactoryReset,  // Giữ 20s: xoá token + WiFi
};

const char* buttonActionText(ButtonAction action);

using ButtonActionHandler = void (*)(ButtonAction action);

// Nút vật lý duy nhất trên mạch. Mốc giữ đặt trong config.h (BUTTON_*_HOLD_MS).
class UserButton {
public:
    UserButton();
    void begin(ButtonActionHandler handler);
    void update(uint32_t nowMs);

    bool isPressed() const { return detector_.isPressed(); }
    // Hành động sẽ chạy nếu nhả tay lúc này – để OLED hiện "Tha tay: ..."
    ButtonAction actionIfReleased(uint32_t nowMs) const;

private:
    ButtonDetector detector_;
    ButtonActionHandler handler_ = nullptr;
};
