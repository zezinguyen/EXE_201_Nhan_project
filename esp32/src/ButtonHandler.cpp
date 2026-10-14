#include "ButtonHandler.h"
#include "PowerManager.h"
#include "DisplayOled.h"

void ButtonHandler::begin() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    lastState = digitalRead(PIN_BUTTON);
}

ButtonEvent ButtonHandler::update() {
    bool currentState = digitalRead(PIN_BUTTON);
    unsigned long now = millis();
    ButtonEvent event = BTN_NONE;

    // Nhấn xuống (LOW do nối đất)
    if (currentState == LOW && lastState == HIGH) {
        pressStartTime = now;
        isPressed = true;
        eventDispatched = false;
        PowerManager::instance().wakeScreen();
    }
    // Đang giữ nút
    else if (currentState == LOW && isPressed) {
        unsigned long duration = now - pressStartTime;
        // Hiển thị đếm giây lên OLED khi người dùng giữ nút
        if (duration >= 20000 && !eventDispatched) {
            DisplayOled::instance().showTemporaryMessage("Giu > 20s", "Khoi phuc goc...", 1000);
        } else if (duration >= 10000 && !eventDispatched) {
            DisplayOled::instance().showTemporaryMessage("Giu > 10s", "Che do Ghep doi...", 1000);
        } else if (duration >= 5000 && !eventDispatched) {
            DisplayOled::instance().showTemporaryMessage("Giu > 5s", "Hoc lenh tu nut...", 1000);
        }
    }
    // Thả nút (chuyển từ LOW lên HIGH)
    else if (currentState == HIGH && lastState == LOW) {
        unsigned long duration = now - pressStartTime;
        isPressed = false;

        if (duration >= 20000) {
            event = BTN_HOLD_20S_RESET;
        } else if (duration >= 10000) {
            event = BTN_HOLD_10S_PAIRING;
        } else if (duration >= 5000) {
            event = BTN_HOLD_5S_LEARN;
        } else if (duration >= 50) {
            // Nhấn ngắn chống dội phím (debounce > 50ms)
            event = BTN_SHORT_PRESS;
        }
    }

    lastState = currentState;
    return event;
}

