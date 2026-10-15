#include "ButtonDetector.h"

ButtonDetector::ButtonDetector(uint16_t debounceMs, const uint32_t* holdLevelsMs, uint8_t levelCount)
    : debounceMs_(debounceMs) {
    if (holdLevelsMs == nullptr) {
        return;
    }
    for (uint8_t i = 0; i < levelCount && i < kMaxHoldLevels; i++) {
        holdLevelsMs_[levelCount_++] = holdLevelsMs[i];
    }
}

ButtonEvent ButtonDetector::update(bool rawPressed, uint32_t nowMs) {
    if (rawPressed != lastRaw_) {
        lastRaw_ = rawPressed;
        lastChangeMs_ = nowMs;
    }
    if (rawPressed == stablePressed_ || nowMs - lastChangeMs_ < debounceMs_) {
        return {PressType::None, 0};
    }

    stablePressed_ = rawPressed;
    if (stablePressed_) {
        pressStartMs_ = lastChangeMs_;
        return {PressType::None, 0};
    }

    const uint8_t level = holdLevelFor(lastChangeMs_ - pressStartMs_);
    return level == 0 ? ButtonEvent{PressType::Short, 0} : ButtonEvent{PressType::Hold, level};
}

uint32_t ButtonDetector::heldMs(uint32_t nowMs) const {
    return stablePressed_ ? nowMs - pressStartMs_ : 0;
}

uint8_t ButtonDetector::currentHoldLevel(uint32_t nowMs) const {
    return stablePressed_ ? holdLevelFor(heldMs(nowMs)) : 0;
}

uint8_t ButtonDetector::holdLevelFor(uint32_t heldMs) const {
    uint8_t level = 0;
    while (level < levelCount_ && heldMs >= holdLevelsMs_[level]) {
        level++;
    }
    return level;
}
