#pragma once
#include <Arduino.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>
#include "config.h"
#include "IrCode.h"

enum LearnOrigin {
    LEARN_FROM_APP,     // Gán phím từ app (có requestId)
    LEARN_FROM_BUTTON   // Học bằng giữ nút 5s (lưu vào learned_codes)
};

class IrLearner {
public:
    static IrLearner& instance() {
        static IrLearner learner;
        return learner;
    }

    void begin();
    void startLearning(LearnOrigin origin, const String& requestId = "");
    void cancelLearning();
    void update();

    bool isLearning() const { return active; }
    LearnOrigin getOrigin() const { return currentOrigin; }
    String getRequestId() const { return currentRequestId; }

private:
    IrLearner();

    IRrecv irRecv;
    decode_results results;

    bool active = false;
    LearnOrigin currentOrigin = LEARN_FROM_APP;
    String currentRequestId = "";

    int step = 0; // 1 = chờ lần 1, 2 = chờ lần 2
    int mismatchCount = 0;
    unsigned long startTime = 0;

    IrCode firstCapture;

    void powerSensor(bool on);
    bool compareCaptures(const IrCode& a, const IrCode& b);
    void finishSuccess(const IrCode& code);
    void finishFailure(const String& reason);
};

