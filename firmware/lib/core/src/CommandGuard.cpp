#include "CommandGuard.h"

#include <string.h>

const char* commandVerdictName(CommandVerdict verdict) {
    switch (verdict) {
        case CommandVerdict::Accept: return "ACCEPT";
        case CommandVerdict::Expired: return "E_EXPIRED";
        case CommandVerdict::Duplicate: return "DUPLICATE";
        case CommandVerdict::StaleSequence: return "E_STALE_SEQ";
        case CommandVerdict::RateLimited: return "E_RATE_LIMITED";
    }
    return "UNKNOWN";
}

CommandGuard::CommandGuard(uint8_t rateMax, uint32_t rateWindowMs) : rateLimiter_(rateMax, rateWindowMs) {}

CommandVerdict CommandGuard::check(const char* commandId, int64_t expiresAtEpochMs, uint32_t seq,
                                   int64_t nowEpochMs, bool clockSynced, uint32_t uptimeMs) {
    if (commandId != nullptr && indexOf(commandId) >= 0) {
        return CommandVerdict::Duplicate;
    }

    if (clockSynced) {
        if (nowEpochMs > expiresAtEpochMs) {
            return CommandVerdict::Expired;
        }
    } else if (hasSeq_ && seq <= lastSeq_) {
        return CommandVerdict::StaleSequence;
    }

    if (!rateLimiter_.tryAcquire(uptimeMs)) {
        return CommandVerdict::RateLimited;
    }

    if (commandId != nullptr) {
        rememberId(commandId);
    }
    if (!hasSeq_ || seq > lastSeq_) {
        lastSeq_ = seq;
        hasSeq_ = true;
    }
    return CommandVerdict::Accept;
}

void CommandGuard::reset() {
    memset(history_, 0, sizeof(history_));
    memset(hasResult_, 0, sizeof(hasResult_));
    historyNext_ = 0;
    lastSeq_ = 0;
    hasSeq_ = false;
    rateLimiter_.reset();
}

void CommandGuard::setResult(const char* commandId, const char* error) {
    const int8_t index = commandId != nullptr ? indexOf(commandId) : -1;
    if (index >= 0) {
        results_[index] = error;
        hasResult_[index] = true;
    }
}

bool CommandGuard::findResult(const char* commandId, const char*& error) const {
    const int8_t index = commandId != nullptr ? indexOf(commandId) : -1;
    if (index < 0 || !hasResult_[index]) {
        return false;
    }
    error = results_[index];
    return true;
}

int8_t CommandGuard::indexOf(const char* commandId) const {
    if (commandId[0] == '\0') {
        return -1;
    }
    for (uint8_t i = 0; i < kHistorySize; i++) {
        if (strncmp(history_[i], commandId, kMaxIdLength) == 0) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;
}

void CommandGuard::rememberId(const char* commandId) {
    strncpy(history_[historyNext_], commandId, kMaxIdLength);
    history_[historyNext_][kMaxIdLength] = '\0';
    hasResult_[historyNext_] = false;
    historyNext_ = (historyNext_ + 1) % kHistorySize;
}
