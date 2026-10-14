#pragma once
#include <Arduino.h>
#include "config.h"

enum ButtonEvent {
    BTN_NONE,
    BTN_SHORT_PRESS,      // Bấm ngắn: sáng OLED
    BTN_HOLD_5S_LEARN,    // Giữ 5s: học lệnh bằng nút
    BTN_HOLD_10S_PAIRING, // Giữ 10s: cài đặt WiFi & ghép đôi
    BTN_HOLD_20S_RESET    // Giữ 20s: khôi phục cài đặt gốc
};

class ButtonHandler {
public:
    static ButtonHandler& instance() {
        static ButtonHandler handler;
        return handler;
    }

    void begin();
    ButtonEvent update();

private:
    ButtonHandler() = default;

    bool lastState = HIGH;
    unsigned long pressStartTime = 0;
    bool isPressed = false;
    bool eventDispatched = false;
};

