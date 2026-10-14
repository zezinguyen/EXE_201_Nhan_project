#include "DisplayOled.h"
#include "PowerManager.h"
#include <WiFi.h>

DisplayOled::DisplayOled() 
    : oledDisplay(OLED_I2C_ADDR, PIN_OLED_SDA, PIN_OLED_SCL) {}

void DisplayOled::begin() {
    oledDisplay.init();
    oledDisplay.flipScreenVertically();
    oledDisplay.setFont(ArialMT_Plain_10);
    oledDisplay.clear();
    oledDisplay.drawString(0, 20, "SMART HOME IR v0.1");
    oledDisplay.display();
}

void DisplayOled::setScreenPower(bool on) {
    screenPowered = on;
    if (on) {
        oledDisplay.displayOn();
    } else {
        oledDisplay.displayOff();
    }
}

void DisplayOled::showStatus(const String& mainText, const String& subText) {
    currentState = OLED_STATE_NORMAL;
    lockAwake = false;
    statusMain = mainText;
    statusSub = subText;
}

void DisplayOled::showLearningPrompt(int step, const String& info) {
    currentState = OLED_STATE_LEARNING;
    lockAwake = true;
    learnStep = step;
    learnInfo = info;
    PowerManager::instance().wakeScreen();
}

void DisplayOled::showPairingInfo(const String& apSsid, const String& apPass, const String& code) {
    currentState = OLED_STATE_PAIRING;
    lockAwake = true;
    pairingSsid = apSsid;
    pairingPass = apPass;
    pairingCode = code;
    PowerManager::instance().wakeScreen();
}

void DisplayOled::showTemporaryMessage(const String& line1, const String& line2, uint32_t durationMs) {
    tempLine1 = line1;
    tempLine2 = line2;
    tempMsgExpire = millis() + durationMs;
    PowerManager::instance().wakeScreen();
}

void DisplayOled::drawHeader() {
    int bat = PowerManager::instance().getBatteryPercent();
    bool chrg = PowerManager::instance().isCharging();
    bool usb = PowerManager::instance().isUsbConnected();

    // Hiển thị Pin & Trạng thái sạc
    String batStr = String(bat) + "%";
    if (chrg) batStr += "+";
    else if (usb) batStr += "=";
    oledDisplay.drawString(0, 0, batStr);

    // Hiển thị RSSI sóng WiFi
    if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        String wifiStr = String(rssi) + "dBm";
        oledDisplay.drawString(128 - oledDisplay.getStringWidth(wifiStr), 0, wifiStr);
    } else {
        oledDisplay.drawString(128 - oledDisplay.getStringWidth("No WiFi"), 0, "No WiFi");
    }

    oledDisplay.drawHorizontalLine(0, 13, 128);
}

void DisplayOled::update() {
    if (!screenPowered) return;

    oledDisplay.clear();
    drawHeader();

    unsigned long now = millis();
    if (now < tempMsgExpire) {
        // Đang có tin nhắn thông báo tạm thời
        oledDisplay.setFont(ArialMT_Plain_10);
        oledDisplay.drawString(0, 20, tempLine1);
        if (!tempLine2.isEmpty()) {
            oledDisplay.drawString(0, 38, tempLine2);
        }
    } else if (currentState == OLED_STATE_PAIRING) {
        // Màn hình ghép đôi & WiFi riêng (Spec 10.2)
        oledDisplay.setFont(ArialMT_Plain_10);
        oledDisplay.drawString(0, 16, "AP: " + pairingSsid);
        oledDisplay.drawString(0, 28, "Pass: " + pairingPass);
        
        oledDisplay.drawString(0, 42, "Ma ghep:");
        oledDisplay.setFont(ArialMT_Plain_16);
        oledDisplay.drawString(55, 40, pairingCode);
    } else if (currentState == OLED_STATE_LEARNING) {
        // Màn hình học lệnh remote (Spec 7.1)
        oledDisplay.setFont(ArialMT_Plain_10);
        oledDisplay.drawString(0, 18, "HOC LENH TU REMOTE");
        if (learnStep == 1) {
            oledDisplay.drawString(0, 32, "Lan 1: Bam phim remote");
        } else if (learnStep == 2) {
            oledDisplay.drawString(0, 32, "Lan 2: Bam lai de so!");
        } else {
            oledDisplay.drawString(0, 32, learnInfo);
        }
        oledDisplay.drawString(0, 48, "Cho 15s...");
    } else {
        // Màn hình trạng thái bình thường
        oledDisplay.setFont(ArialMT_Plain_16);
        oledDisplay.drawString(0, 20, statusMain);
        oledDisplay.setFont(ArialMT_Plain_10);
        if (!statusSub.isEmpty()) {
            oledDisplay.drawString(0, 44, statusSub);
        }
    }

    oledDisplay.display();
}

