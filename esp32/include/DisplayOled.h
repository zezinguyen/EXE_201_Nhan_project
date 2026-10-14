#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include "config.h"

enum OledState {
    OLED_STATE_NORMAL,
    OLED_STATE_LEARNING,
    OLED_STATE_PAIRING,
    OLED_STATE_MESSAGE
};

class DisplayOled {
public:
    static DisplayOled& instance() {
        static DisplayOled display;
        return display;
    }

    void begin();
    void update();
    void setScreenPower(bool on);

    void showStatus(const String& mainText, const String& subText = "");
    void showLearningPrompt(int step, const String& info = "");
    void showPairingInfo(const String& apSsid, const String& apPass, const String& code);
    void showTemporaryMessage(const String& line1, const String& line2 = "", uint32_t durationMs = 3000);

    bool isLockAwake() const { return lockAwake; }

private:
    DisplayOled();
    SSD1306Wire oledDisplay;

    OledState currentState = OLED_STATE_NORMAL;
    bool screenPowered = true;
    bool lockAwake = false;

    String statusMain = "Khởi động...";
    String statusSub = "";
    int learnStep = 1;
    String learnInfo = "";

    String pairingSsid = "";
    String pairingPass = "";
    String pairingCode = "";

    unsigned long tempMsgExpire = 0;
    String tempLine1 = "";
    String tempLine2 = "";

    void drawHeader();
};

