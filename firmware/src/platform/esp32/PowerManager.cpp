#include "modules/PowerManager.h"

#include <Arduino.h>
#include <Elapsed.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "config.h"

PowerManager::PowerManager()
    : gauge_({BATTERY_LOW_PERCENT, BATTERY_CRITICAL_PERCENT, BATTERY_EMPTY_PERCENT}, BATTERY_HYSTERESIS_PERCENT),
      filter_(BATTERY_FILTER_SHIFT) {}

void PowerManager::begin() {
    readBattery();  // Trước readCharger(): suy ra "đang sạc" cần điện áp pin
    readCharger();
    lastSampleMs_ = millis();
    applyPowerProfile();
}

void PowerManager::update(uint32_t nowMs) {
    changed_ = false;
    const PowerSource oldSource = source_;
    const BatteryLevel oldLevel = gauge_.level();

    readCharger();
    if (source_ != oldSource) {
        filter_.reset();  // Cắm/rút sạc: điện áp đổi bậc, đo lại từ đầu
        applyPowerProfile();
    }
    if (source_ != oldSource || elapsedMs(nowMs, lastSampleMs_) >= BATTERY_SAMPLE_INTERVAL_MS) {
        lastSampleMs_ = nowMs;
        readBattery();
    }

    changed_ = source_ != oldSource || gauge_.level() != oldLevel;
}

void PowerManager::applyPowerProfile() {
    const bool onBattery = source_ == PowerSource::Battery;
    setCpuFrequencyMhz(onBattery ? CPU_FREQ_BATTERY_MHZ : CPU_FREQ_USB_MHZ);

    if (WiFi.getMode() == WIFI_OFF) {
        return;
    }
    wifi_ps_type_t mode = WIFI_PS_NONE;
    if (onBattery) {
        mode = WIFI_USE_MAX_MODEM_SLEEP ? WIFI_PS_MAX_MODEM : WIFI_PS_MIN_MODEM;
    }
    esp_wifi_set_ps(mode);
    // TODO(giai đoạn 2): esp_pm_configure + light sleep tự động (cần CONFIG_PM_ENABLE)
}

void PowerManager::readCharger() {
    // Đo 5V của cổng sạc qua cầu chia (digitalRead không đủ tin cậy ở mức 2.5V)
    const bool usb = analogReadMilliVolts(PIN_USB_SENSE) >= USB_SENSE_MIN_MV;
    source_ = usb ? PowerSource::Usb : PowerSource::Battery;
    charging_ = usb && millivolts_ < BATTERY_FULL_MV;
}

void PowerManager::readBattery() {
    uint16_t samples[BATTERY_SAMPLES];
    for (uint8_t i = 0; i < BATTERY_SAMPLES; i++) {
        samples[i] = analogReadMilliVolts(PIN_BATTERY_ADC);
    }
    rawMillivolts_ = static_cast<uint16_t>(medianMillivolts(samples, BATTERY_SAMPLES) * BATTERY_DIVIDER_RATIO);
    millivolts_ = filter_.add(rawMillivolts_);
    percent_ = BatteryGauge::percentFromMillivolts(millivolts_);
    gauge_.update(percent_);
}
