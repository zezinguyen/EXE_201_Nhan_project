#include "RateLimiter.h"

RateLimiter::RateLimiter(uint8_t maxEvents, uint32_t windowMs)
    : maxEvents_(maxEvents == 0 ? 1 : (maxEvents > kMaxEvents ? kMaxEvents : maxEvents)),
      windowMs_(windowMs) {}

bool RateLimiter::tryAcquire(uint32_t nowMs) {
    uint8_t inWindow = 0;
    for (uint8_t i = 0; i < count_; i++) {
        if (nowMs - timestamps_[i] < windowMs_) {
            inWindow++;
        }
    }
    if (inWindow >= maxEvents_) {
        return false;
    }

    // Ghi đè mốc cũ nhất (vòng tròn)
    timestamps_[next_] = nowMs;
    next_ = (next_ + 1) % maxEvents_;
    if (count_ < maxEvents_) {
        count_++;
    }
    return true;
}

void RateLimiter::reset() {
    count_ = 0;
    next_ = 0;
}
