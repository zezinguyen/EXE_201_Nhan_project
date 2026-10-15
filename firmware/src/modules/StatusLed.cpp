#include "StatusLed.h"

#include <Arduino.h>

#include "config.h"

namespace {
// Bit màu: 1 = đỏ, 2 = xanh lá, 4 = xanh dương
enum Color : uint8_t {
    kBlack = 0,
    kRed = 1,
    kGreen = 2,
    kYellow = kRed | kGreen,
    kBlue = 4,
    kMagenta = kRed | kBlue,
};

// steps: thời lượng (ms) xen kẽ BẬT/TẮT, bắt đầu bằng BẬT. 0 = hết chuỗi.
struct PatternDef {
    uint8_t color;
    uint16_t steps[6];
};

// Thứ tự trùng với enum LedPattern
const PatternDef kPatterns[] = {
    {kBlack, {0}},                            // Off
    {kGreen, {40, 3000, 0}},                  // Online
    {kYellow, {400, 1600, 0}},                // NoInternet
    {kRed, {100, 100, 0}},                    // NoWifi
    {kBlue, {100, 150, 100, 1000, 0}},        // SetupMode
    {kMagenta, {300, 300, 0}},                // Learning
    {kRed, {60, 150, 60, 150, 60, 2500}},     // LowBattery
};

const uint8_t kPins[] = {PIN_LED_RED, PIN_LED_GREEN, PIN_LED_BLUE};
}  // namespace

void StatusLed::begin() {
    for (uint8_t pin : kPins) {
        pinMode(pin, OUTPUT);
    }
    write(false);
}

void StatusLed::setPattern(LedPattern pattern) {
    if (pattern == pattern_) {
        return;
    }
    pattern_ = pattern;
    step_ = 0;
    stepStartMs_ = millis();
    write(pattern != LedPattern::Off);
}

void StatusLed::update(uint32_t nowMs) {
    if (pattern_ == LedPattern::Off) {
        return;
    }
    const uint16_t* steps = kPatterns[static_cast<uint8_t>(pattern_)].steps;
    if (nowMs - stepStartMs_ < steps[step_]) {
        return;
    }
    stepStartMs_ = nowMs;
    step_++;
    if (step_ >= 6 || steps[step_] == 0) {
        step_ = 0;
    }
    write(step_ % 2 == 0);
}

void StatusLed::write(bool on) {
    const uint8_t color = on ? kPatterns[static_cast<uint8_t>(pattern_)].color : static_cast<uint8_t>(kBlack);
    for (uint8_t i = 0; i < 3; i++) {
        const bool lit = (color >> i) & 1;
        digitalWrite(kPins[i], lit == STATUS_LED_ACTIVE_HIGH ? HIGH : LOW);
    }
}
