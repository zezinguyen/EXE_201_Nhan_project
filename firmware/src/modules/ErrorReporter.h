#pragma once
#include <ErrorCodes.h>
#include <ErrorThrottle.h>
#include <RingBuffer.h>
#include <stdint.h>

#include "config.h"

struct ErrorRecord {
    uint32_t epochSec;
    ErrorCode code;
    char deviceId[32];
};

// Ghi lỗi, lọc theo quy tắc chống spam, xếp hàng chờ gửi bù khi có mạng (spec 8).
// Báo cáo KHÔNG chứa tên WiFi, mật khẩu, thông tin cá nhân.
class ErrorReporter {
public:
    ErrorReporter();

    void report(ErrorCode code, const char* deviceId, uint32_t nowSec, bool userReported = false);

    bool hasPending() const { return queue_.size() > 0; }
    bool peekPending(ErrorRecord& out) const { return queue_.peek(out); }
    void dropPending();

    // TODO(tuần 6): lưu hàng đợi vào LittleFS (ghi vòng) để không mất khi khởi động lại

private:
    ErrorThrottle throttle_;
    RingBuffer<ErrorRecord, ERROR_LOG_CAPACITY> queue_;
};
