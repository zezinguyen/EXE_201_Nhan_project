#include "ErrorThrottle.h"

uint32_t hashKey(const char* text) {
    uint32_t hash = 2166136261u;
    if (text == nullptr) {
        return hash;
    }
    while (*text != '\0') {
        hash ^= static_cast<uint8_t>(*text++);
        hash *= 16777619u;
    }
    return hash;
}

ErrorThrottle::ErrorThrottle(uint8_t repeatThreshold, uint32_t repeatWindowSec, uint32_t minResendSec)
    : repeatThreshold_(repeatThreshold), repeatWindowSec_(repeatWindowSec), minResendSec_(minResendSec) {}

bool ErrorThrottle::shouldReport(ErrorCode code, uint32_t deviceKey, uint32_t nowSec, bool userReported) {
    Slot& slot = findOrCreate(code, deviceKey, nowSec);
    slot.lastSeenSec = nowSec;

    if (nowSec - slot.windowStartSec >= repeatWindowSec_) {
        slot.windowStartSec = nowSec;
        slot.count = 0;
    }
    if (slot.count < UINT8_MAX) {
        slot.count++;
    }

    if (!userReported && slot.count < repeatThreshold_) {
        return false;
    }
    if (slot.sent && nowSec - slot.lastSentSec < minResendSec_) {
        return false;
    }
    slot.sent = true;
    slot.lastSentSec = nowSec;
    return true;
}

ErrorThrottle::Slot& ErrorThrottle::findOrCreate(ErrorCode code, uint32_t deviceKey, uint32_t nowSec) {
    Slot* oldest = &slots_[0];
    for (Slot& slot : slots_) {
        if (slot.used && slot.code == code && slot.deviceKey == deviceKey) {
            return slot;
        }
        if (!slot.used) {
            oldest = &slot;
            break;
        }
        if (slot.lastSeenSec < oldest->lastSeenSec) {
            oldest = &slot;
        }
    }
    *oldest = Slot{true, false, code, deviceKey, nowSec, 0, nowSec, 0};
    return *oldest;
}
