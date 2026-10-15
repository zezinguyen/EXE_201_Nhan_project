#pragma once
#include <BatteryFilter.h>
#include <BatteryGauge.h>
#include <stdint.h>

enum class PowerSource : uint8_t { Usb, Battery };

// Đo pin, nhận biết cắm sạc, áp dụng chế độ tiết kiệm điện (spec 3.4, 5.2).
// Triển khai riêng theo MCU: src/platform/<mcu>/PowerManager.cpp
class PowerManager {
public:
    PowerManager();

    void begin();
    void update(uint32_t nowMs);

    // Gọi lại sau khi WiFi bật/kết nối (Modem-Sleep chỉ áp dụng được khi WiFi chạy).
    void applyPowerProfile();

    PowerSource source() const { return source_; }
    bool isCharging() const { return charging_; }
    uint16_t batteryMillivolts() const { return millivolts_; }  // Đã lọc
    uint16_t batteryRawMillivolts() const { return rawMillivolts_; }  // Lần đo gần nhất, chưa lọc
    uint8_t batteryPercent() const { return percent_; }
    BatteryLevel batteryLevel() const { return gauge_.level(); }
    // true trong lần update() mà nguồn hoặc mức pin vừa thay đổi
    bool changed() const { return changed_; }

private:
    void readCharger();
    void readBattery();

    BatteryGauge gauge_;
    BatteryFilter filter_;
    PowerSource source_ = PowerSource::Battery;
    bool charging_ = false;
    uint16_t millivolts_ = 0;
    uint16_t rawMillivolts_ = 0;
    uint8_t percent_ = 0;
    uint32_t lastSampleMs_ = 0;
    bool changed_ = false;
};
