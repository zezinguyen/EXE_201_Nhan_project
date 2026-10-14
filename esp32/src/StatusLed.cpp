#include "StatusLed.h"

void StatusLed::begin() {
    pinMode(PIN_RGB_R, OUTPUT);
    pinMode(PIN_RGB_G, OUTPUT);
    pinMode(PIN_RGB_B, OUTPUT);
    // Tắt hết LED lúc khởi động (Common Anode: HIGH = TẮT)
    setRgb(false, false, false);
}

void StatusLed::setRgb(bool r, bool g, bool b) {
    // LED RGB Anode chung: mức LOW là SÁNG, mức HIGH là TẮT
    digitalWrite(PIN_RGB_R, r ? LOW : HIGH);
    digitalWrite(PIN_RGB_G, g ? LOW : HIGH);
    digitalWrite(PIN_RGB_B, b ? LOW : HIGH);
}

void StatusLed::setPattern(LedPattern pattern) {
    if (currentPattern != pattern) {
        currentPattern = pattern;
        step = 0;
        lastToggleTime = millis();
    }
}

void StatusLed::update() {
    unsigned long now = millis();

    switch (currentPattern) {
        case LED_ONLINE:
            // 🟢 Xanh lá: nháy ngắn 100ms mỗi 3000ms
            if (now - lastToggleTime >= (step == 0 ? 2900 : 100)) {
                lastToggleTime = now;
                step = !step;
                setRgb(false, step == 1, false);
            }
            break;

        case LED_WIFI_NO_INTERNET:
            // 🟡 Vàng (Đỏ + Xanh lá): nháy chậm 1000ms bật / 1000ms tắt
            if (now - lastToggleTime >= 1000) {
                lastToggleTime = now;
                step = !step;
                setRgb(step == 1, step == 1, false);
            }
            break;

        case LED_NO_WIFI:
            // 🔴 Đỏ: nháy nhanh 200ms bật / 200ms tắt
            if (now - lastToggleTime >= 200) {
                lastToggleTime = now;
                step = !step;
                setRgb(step == 1, false, false);
            }
            break;

        case LED_SETUP_PAIRING:
            // 🔵 Xanh dương: 2 nháy liên tục rồi nghỉ 1.5s
            if (step == 0 && now - lastToggleTime >= 1500) {
                step = 1; lastToggleTime = now; setRgb(false, false, true);
            } else if (step == 1 && now - lastToggleTime >= 150) {
                step = 2; lastToggleTime = now; setRgb(false, false, false);
            } else if (step == 2 && now - lastToggleTime >= 150) {
                step = 3; lastToggleTime = now; setRgb(false, false, true);
            } else if (step == 3 && now - lastToggleTime >= 150) {
                step = 0; lastToggleTime = now; setRgb(false, false, false);
            }
            break;

        case LED_LEARNING:
            // 🟣 Tím (Đỏ + Xanh dương): nháy đều 300ms
            if (now - lastToggleTime >= 300) {
                lastToggleTime = now;
                step = !step;
                setRgb(step == 1, false, step == 1);
            }
            break;

        case LED_LOW_BATTERY:
            // 🔴 Đỏ: 3 nháy ngắn rồi nghỉ 2s
            if (step == 0 && now - lastToggleTime >= 2000) {
                step = 1; lastToggleTime = now; setRgb(true, false, false);
            } else if (step == 1 && now - lastToggleTime >= 100) {
                step = 2; lastToggleTime = now; setRgb(false, false, false);
            } else if (step == 2 && now - lastToggleTime >= 100) {
                step = 3; lastToggleTime = now; setRgb(true, false, false);
            } else if (step == 3 && now - lastToggleTime >= 100) {
                step = 4; lastToggleTime = now; setRgb(false, false, false);
            } else if (step == 4 && now - lastToggleTime >= 100) {
                step = 5; lastToggleTime = now; setRgb(true, false, false);
            } else if (step == 5 && now - lastToggleTime >= 100) {
                step = 0; lastToggleTime = now; setRgb(false, false, false);
            }
            break;
    }
}

