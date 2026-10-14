#include "PowerManager.h"
#include "AudioPlayer.h"
#include "DisplayOled.h"

void PowerManager::begin() {
    // 1. Hạ xung CPU xuống 80MHz để tiết kiệm điện (Spec 5.2)
    setCpuFrequencyMhz(CPU_FREQ_MHZ);

    // 2. Cấu hình chân đo ADC
    pinMode(PIN_BATTERY_ADC, INPUT);
    pinMode(PIN_USB_ADC, INPUT);

    // 3. Đặt độ phân giải ADC 12 bit (0 - 4095) và suy giảm 11dB (đo tới ~3.6V)
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    wakeScreen();
    readAdc();
}

void PowerManager::wakeScreen() {
    screenAwake = true;
    lastScreenWakeTime = millis();
    DisplayOled::instance().setScreenPower(true);
}

void PowerManager::update() {
    unsigned long now = millis();

    // Đọc điện áp định kỳ mỗi 5 giây
    if (now - lastAdcReadTime >= 5000) {
        lastAdcReadTime = now;
        readAdc();
    }

    // Tự động tắt màn hình OLED sau 10s không dùng (Spec 5.3)
    // Nếu đang trong chế độ học lệnh hoặc cài đặt thì luôn giữ sáng
    if (screenAwake && (now - lastScreenWakeTime >= OLED_SLEEP_TIMEOUT_MS)) {
        if (!DisplayOled::instance().isLockAwake()) {
            screenAwake = false;
            DisplayOled::instance().setScreenPower(false);
        }
    }
}

void PowerManager::readAdc() {
    // Đọc trung bình mẫu ADC để giảm nhiễu
    uint32_t rawBat = 0;
    uint32_t rawUsb = 0;
    const int SAMPLES = 8;
    for (int i = 0; i < SAMPLES; i++) {
        rawBat += analogRead(PIN_BATTERY_ADC);
        rawUsb += analogRead(PIN_USB_ADC);
        delayMicroseconds(200);
    }
    rawBat /= SAMPLES;
    rawUsb /= SAMPLES;

    // Hệ số hiệu chuẩn cho cầu chia 100k / 100k: V_in = V_adc * 2 * (3.3V / 4095) * calib
    const float calibFactor = 1.05f; 
    float vBat = (rawBat / 4095.0f) * 3.3f * 2.0f * calibFactor;
    float vUsb = (rawUsb / 4095.0f) * 3.3f * 2.0f * calibFactor;

    batteryVoltage = vBat;
    usbConnected = (vUsb >= 4.2f);

    // Xác định đang sạc: cắm USB và pin chưa đầy (< 4.15V) (Spec 3.3)
    charging = (usbConnected && (vBat < 4.15f));

    // Tính % pin (LiPo: 3.3V = 0%, 4.2V = 100%)
    if (vBat <= 3.30f) {
        batteryPercent = 0;
    } else if (vBat >= 4.20f) {
        batteryPercent = 100;
    } else {
        batteryPercent = (int)((vBat - 3.30f) / (4.20f - 3.30f) * 100.0f);
    }

    // Cảnh báo pin yếu khi chạy pin (<= 20%)
    if (!usbConnected && batteryPercent <= 20 && !lowBatteryPromptPlayed) {
        lowBatteryPromptPlayed = true;
        AudioPlayer::instance().playTrack(TRACK_LOW_BATTERY);
    } else if (batteryPercent > 25) {
        lowBatteryPromptPlayed = false;
    }
}

