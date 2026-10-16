#pragma once
#include <stdint.h>

enum class LedPattern : uint8_t {
    Off,
    Online,      // Xanh lá, nháy ngắn mỗi vài giây
    NoInternet,  // Vàng, nháy chậm
    NoWifi,      // Đỏ, nháy nhanh
    SetupMode,   // Xanh dương, 2 nháy liên tục
    Learning,    // Tím, nháy đều – đang chờ bấm phím remote
    LowBattery,  // Đỏ, 3 nháy ngắn
};

// LED RGB trạng thái, nháy theo mẫu không chặn (non-blocking), độ sáng PWM STATUS_LED_BRIGHTNESS. Màu xem spec 9.3.
class StatusLed {
public:
    void begin();
    void setPattern(LedPattern pattern);
    void update(uint32_t nowMs);

private:
    void write(bool on);

    LedPattern pattern_ = LedPattern::Off;
    uint8_t step_ = 0;
    uint32_t stepStartMs_ = 0;
};
