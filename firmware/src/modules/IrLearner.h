#pragma once
#include <ArduinoJson.h>
#include <stdint.h>

#include "config.h"

struct IrCapture {
    char protocol[24];  // "UNKNOWN" nếu không giải mã được -> dùng raw
    uint64_t value;
    uint16_t bits;
    uint16_t raw[IR_RAW_MAX_LENGTH];
    uint16_t rawLength;
};

// Ghi mã đã thu ra JSON: {protocol, code, bits} hoặc {protocol: "UNKNOWN", raw, freq}.
// code là chuỗi hex vì JS không giữ đủ chính xác số 64 bit.
void irCaptureToJson(const IrCapture& capture, JsonObject out);

enum class LearnEvent : uint8_t {
    None,
    FirstCaptured,  // Đã thu lần 1, chờ bấm lại lần 2
    Mismatch,       // 2 lần khác nhau -> yêu cầu bấm lại từ đầu
    Failed,         // Lệch quá IR_LEARN_MAX_FAILS lần, đã tắt mắt thu (E_LEARN_FAIL)
    Success,        // result() có dữ liệu
    Timeout,        // Quá IR_LEARN_TIMEOUT_MS, đã tắt mắt thu
};

// Gán phím: bật mắt thu, thu 2 lần và so sánh (spec 7.2).
class IrLearner {
public:
    void begin();
    void start(uint32_t nowMs);
    void stop();
    LearnEvent update(uint32_t nowMs);
    bool isActive() const { return active_; }
    const IrCapture& result() const { return captures_[0]; }

private:
    bool readCapture(IrCapture& out);
    bool capturesMatch() const;
    void setPower(bool on);

    IrCapture captures_[2] = {};
    uint8_t captureCount_ = 0;
    uint8_t mismatches_ = 0;
    bool active_ = false;
    uint32_t startMs_ = 0;
};
