#include "Backoff.h"

Backoff::Backoff(uint32_t baseMs, uint32_t maxMs, uint8_t jitterPercent)
    : baseMs_(baseMs), maxMs_(maxMs < baseMs ? baseMs : maxMs), currentMs_(baseMs),
      jitterPercent_(jitterPercent > 100 ? 100 : jitterPercent) {}

uint32_t Backoff::nextDelayMs(uint32_t randomValue) {
    uint32_t delayMs = currentMs_;
    const uint32_t jitter = static_cast<uint32_t>(static_cast<uint64_t>(delayMs) * jitterPercent_ / 100);
    if (jitter > 0) {
        delayMs = delayMs - jitter + randomValue % (2 * jitter + 1);
    }

    currentMs_ = (currentMs_ >= maxMs_ / 2) ? maxMs_ : currentMs_ * 2;
    if (attempts_ < UINT16_MAX) {
        attempts_++;
    }
    return delayMs;
}

void Backoff::reset() {
    currentMs_ = baseMs_;
    attempts_ = 0;
}
