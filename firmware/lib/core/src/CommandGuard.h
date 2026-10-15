#pragma once
#include <stddef.h>
#include <stdint.h>

#include "RateLimiter.h"

enum class CommandVerdict : uint8_t {
    Accept,
    Expired,        // Quá hạn expiresAt
    Duplicate,      // commandId đã xử lý -> không phát lại, chỉ ACK lại
    StaleSequence,  // Chưa có giờ NTP và seq không lớn hơn lệnh gần nhất
    RateLimited,    // Quá nhiều lệnh trong thời gian ngắn
};

const char* commandVerdictName(CommandVerdict verdict);

// Kiểm tra một lệnh có được thực thi không (spec 4.1 + 10.1):
// chống trùng commandId, hạn dùng, số thứ tự khi chưa có giờ, giới hạn tốc độ.
class CommandGuard {
public:
    static constexpr size_t kMaxIdLength = 40;
    static constexpr uint8_t kHistorySize = 16;

    CommandGuard(uint8_t rateMax, uint32_t rateWindowMs);

    // nowEpochMs chỉ dùng khi clockSynced = true. uptimeMs dùng cho giới hạn tốc độ.
    CommandVerdict check(const char* commandId, int64_t expiresAtEpochMs, uint32_t seq,
                         int64_t nowEpochMs, bool clockSynced, uint32_t uptimeMs);
    void reset();

    // Lưu kết quả của lệnh đã Accept để ACK lại đúng khi app gửi trùng.
    // error phải là chuỗi tồn tại suốt chương trình (hằng), nullptr = thành công.
    void setResult(const char* commandId, const char* error);
    // false nếu không còn nhớ commandId (hoặc chưa có kết quả)
    bool findResult(const char* commandId, const char*& error) const;

private:
    int8_t indexOf(const char* commandId) const;
    void rememberId(const char* commandId);

    char history_[kHistorySize][kMaxIdLength + 1] = {};
    const char* results_[kHistorySize] = {};
    bool hasResult_[kHistorySize] = {};
    uint8_t historyNext_ = 0;
    uint32_t lastSeq_ = 0;
    bool hasSeq_ = false;
    RateLimiter rateLimiter_;
};
