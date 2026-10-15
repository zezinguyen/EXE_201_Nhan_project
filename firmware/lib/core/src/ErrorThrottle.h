#pragma once
#include <stdint.h>

#include "ErrorCodes.h"

// FNV-1a 32 bit: đổi deviceId (chuỗi) thành khoá gọn để so sánh.
uint32_t hashKey(const char* text);

// Quyết định một lỗi có được gửi lên Supabase không (spec mục 8):
//  - Gửi khi lặp >= repeatThreshold lần trong repeatWindowSec, hoặc người dùng báo.
//  - Cùng mã lỗi + cùng thiết bị chỉ gửi 1 lần mỗi minResendSec.
class ErrorThrottle {
public:
    static constexpr uint8_t kSlots = 16;

    ErrorThrottle(uint8_t repeatThreshold, uint32_t repeatWindowSec, uint32_t minResendSec);

    bool shouldReport(ErrorCode code, uint32_t deviceKey, uint32_t nowSec, bool userReported);

private:
    struct Slot {
        bool used;
        bool sent;
        ErrorCode code;
        uint32_t deviceKey;
        uint32_t windowStartSec;
        uint32_t lastSentSec;
        uint32_t lastSeenSec;
        uint8_t count;
    };

    Slot& findOrCreate(ErrorCode code, uint32_t deviceKey, uint32_t nowSec);

    Slot slots_[kSlots] = {};
    uint8_t repeatThreshold_;
    uint32_t repeatWindowSec_;
    uint32_t minResendSec_;
};
