#pragma once
#include <stdint.h>

// Cho phép tối đa maxEvents sự kiện trong một cửa sổ trượt windowMs.
class RateLimiter {
public:
    static constexpr uint8_t kMaxEvents = 16;

    RateLimiter(uint8_t maxEvents, uint32_t windowMs);

    // true = cho phép (và ghi nhận sự kiện), false = vượt giới hạn.
    bool tryAcquire(uint32_t nowMs);
    void reset();

private:
    uint32_t timestamps_[kMaxEvents] = {};
    uint8_t maxEvents_;
    uint8_t count_ = 0;
    uint8_t next_ = 0;
    uint32_t windowMs_;
};
