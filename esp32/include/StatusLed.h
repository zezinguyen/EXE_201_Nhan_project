#pragma once
#include <Arduino.h>
#include "config.h"

enum LedPattern {
    LED_ONLINE,          // 🟢 Xanh lá: nháy ngắn mỗi 3 giây
    LED_WIFI_NO_INTERNET,// 🟡 Vàng: nháy chậm
    LED_NO_WIFI,         // 🔴 Đỏ: nháy nhanh
    LED_SETUP_PAIRING,   // 🔵 Xanh dương: 2 nháy liên tục
    LED_LEARNING,        // 🟣 Tím: nháy đều
    LED_LOW_BATTERY      // 🔴 Đỏ: 3 nháy ngắn
};

class StatusLed {
public:
    static StatusLed& instance() {
        static StatusLed led;
        return led;
    }

    void begin();
    void setPattern(LedPattern pattern);
    void update();

private:
    StatusLed() = default;

    LedPattern currentPattern = LED_NO_WIFI;
    unsigned long lastToggleTime = 0;
    int step = 0;

    void setRgb(bool r, bool g, bool b);
};

