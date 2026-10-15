#include "DisplayOled.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "config.h"

namespace {
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
}

bool DisplayOled::begin() {
    ready_ = oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS);
    if (ready_) {
        oled.clearDisplay();
        oled.setTextColor(SSD1306_WHITE);
        oled.display();
    }
    return ready_;
}

void DisplayOled::wake(uint32_t nowMs) {
    if (!ready_ || disabled_) {
        return;
    }
    wakeMs_ = nowMs;
    if (!awake_) {
        oled.ssd1306_command(SSD1306_DISPLAYON);
        awake_ = true;
    }
}

void DisplayOled::sleep() {
    if (ready_ && awake_) {
        oled.ssd1306_command(SSD1306_DISPLAYOFF);
    }
    awake_ = false;
}

void DisplayOled::update(uint32_t nowMs) {
    if (awake_ && nowMs - wakeMs_ >= OLED_TIMEOUT_MS) {
        sleep();
    }
}

void DisplayOled::show(const DisplayStatus& status) {
    if (!ready_ || !awake_) {
        return;
    }
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print(connectionStateText(status.connection));

    oled.setCursor(80, 0);
    oled.printf("%s%u%%", status.charging ? "+" : "", status.batteryPercent);

    if (status.setupSsid != nullptr) {
        oled.setCursor(0, 14);
        oled.printf("WiFi:%s", status.setupSsid);
        oled.setCursor(0, 26);
        oled.printf("Pass:%s", status.setupPassword);
    }
    if (status.pairingCode != nullptr) {
        oled.setCursor(0, 42);
        oled.print("Ma:");
        oled.setTextSize(2);
        oled.setCursor(24, 42);
        oled.print(status.pairingCode);
        oled.setTextSize(1);
    } else if (status.message != nullptr) {
        oled.setCursor(0, 24);
        oled.print(status.message);
    }
    oled.display();
}

void DisplayOled::setDisabled(bool disabled) {
    disabled_ = disabled;
    if (disabled_) {
        sleep();
    }
}
