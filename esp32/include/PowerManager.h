#pragma once
#include <Arduino.h>
#include <esp_wifi.h>
#include "config.h"

class PowerManager {
public:
    static PowerManager& instance() {
        static PowerManager mgr;
        return mgr;
    }

    void begin();
    void update();

    // Thông số nguồn
    int getBatteryPercent() const { return batteryPercent; }
    float getBatteryVoltage() const { return batteryVoltage; }
    bool isCharging() const { return charging; }
    bool isUsbConnected() const { return usbConnected; }

    // Quản lý sáng màn hình
    void wakeScreen();
    bool isScreenAwake() const { return screenAwake; }

private:
    PowerManager() = default;

    int batteryPercent = 100;
    float batteryVoltage = 4.0f;
    bool charging = false;
    bool usbConnected = false;

    bool screenAwake = true;
    unsigned long lastScreenWakeTime = 0;
    unsigned long lastAdcReadTime = 0;
    bool lowBatteryPromptPlayed = false;

    void readAdc();
};

