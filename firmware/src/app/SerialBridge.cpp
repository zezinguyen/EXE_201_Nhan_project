#include "SerialBridge.h"

#include <Arduino.h>

void SerialBridge::update() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') {
            continue;
        }
        if (c != '\n') {
            if (length_ < sizeof(line_) - 1) {
                line_[length_++] = c;
            } else {
                overflow_ = true;
            }
            continue;
        }
        if (overflow_) {
            log_w("Dong Serial qua dai, bo qua");
        } else if (length_ > 0) {
            line_[length_] = '\0';
            handleLine();
        }
        length_ = 0;
        overflow_ = false;
    }
}

void SerialBridge::handleLine() {
    JsonDocument doc;
    if (deserializeJson(doc, line_, length_) != DeserializationError::Ok) {
        log_w("JSON khong hop le: %s", line_);
        return;
    }
    const char* event = doc["event"];
    if (event == nullptr || handler_ == nullptr) {
        log_w("Thieu \"event\"");
        return;
    }
    log_i("Serial -> %s", event);
    handler_(event, doc["payload"].as<JsonObjectConst>());
}
