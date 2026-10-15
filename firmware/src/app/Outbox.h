#pragma once
#include <RingBuffer.h>
#include <stdint.h>

#include "config.h"
#include "modules/ErrorReporter.h"
#include "modules/IrLearner.h"
#include "modules/SupabaseRest.h"

struct LearnedCode {
    uint32_t epochSec;
    IrCapture capture;
};

// Hàng đợi dữ liệu cần ghi lên Supabase: mã học từ nút (bảng learned_codes, chờ chủ nhà
// gán trong app) và báo lỗi (bảng error_reports). Mất mạng thì giữ lại, có mạng gửi bù.
class Outbox {
public:
    Outbox(SupabaseRest& rest, ErrorReporter& errors);

    // Trả về false nếu hàng đợi đầy và mã cũ nhất bị ghi đè
    bool queueLearnedCode(const IrCapture& capture, uint32_t epochSec);
    size_t pendingLearnedCodes() const { return learnedCodes_.size(); }

    // Gửi tối đa 1 bản ghi mỗi lần gọi; lỗi thì chờ OUTBOX_RETRY_MS.
    void update(uint32_t nowMs, bool online);

    // TODO(tuần 6): lưu hàng đợi vào LittleFS để không mất khi khởi động lại

private:
    bool sendLearnedCode(const LearnedCode& item);
    bool sendError(const ErrorRecord& record);

    SupabaseRest& rest_;
    ErrorReporter& errors_;
    RingBuffer<LearnedCode, LEARNED_CODE_QUEUE_SIZE> learnedCodes_;
    uint32_t retryAtMs_ = 0;
    bool waitingRetry_ = false;
};
