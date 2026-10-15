#include "IrLearner.h"

#include <Arduino.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>
#include <IrCompare.h>
#include <string.h>

namespace {
IRrecv irRecv(PIN_IR_RX, IR_RX_BUFFER_SIZE, IR_RX_TIMEOUT_MS, true);
decode_results decodeResults;
}  // namespace

void irCaptureToJson(const IrCapture& capture, JsonObject out) {
    out["protocol"] = capture.protocol;
    if (strcmp(capture.protocol, "UNKNOWN") == 0) {
        JsonArray raw = out["raw"].to<JsonArray>();
        for (uint16_t i = 0; i < capture.rawLength; i++) {
            raw.add(capture.raw[i]);
        }
        out["freq"] = IR_DEFAULT_KHZ;
        return;
    }
    char code[24];
    snprintf(code, sizeof(code), "0x%llX", static_cast<unsigned long long>(capture.value));
    out["code"] = code;
    out["bits"] = capture.bits;
}

void IrLearner::begin() {
    if (PIN_IR_RX_POWER >= 0) {
        pinMode(PIN_IR_RX_POWER, OUTPUT);
    }
    setPower(false);
}

void IrLearner::start(uint32_t nowMs) {
    setPower(true);
    irRecv.enableIRIn();
    captureCount_ = 0;
    mismatches_ = 0;
    startMs_ = nowMs;
    active_ = true;
}

void IrLearner::stop() {
    if (active_) {
        irRecv.disableIRIn();
    }
    setPower(false);
    active_ = false;
}

LearnEvent IrLearner::update(uint32_t nowMs) {
    if (!active_) {
        return LearnEvent::None;
    }
    if (nowMs - startMs_ >= IR_LEARN_TIMEOUT_MS) {
        stop();
        return LearnEvent::Timeout;
    }
    if (!irRecv.decode(&decodeResults)) {
        return LearnEvent::None;
    }

    const bool ok = readCapture(captures_[captureCount_]);
    irRecv.resume();
    if (!ok) {
        return LearnEvent::None;  // Gói lỗi/tràn bộ đệm -> chờ lần bấm sau
    }

    if (captureCount_ == 0) {
        captureCount_ = 1;
        return LearnEvent::FirstCaptured;
    }
    if (!capturesMatch()) {
        captureCount_ = 0;
        if (++mismatches_ >= IR_LEARN_MAX_FAILS) {
            stop();
            return LearnEvent::Failed;
        }
        return LearnEvent::Mismatch;
    }
    stop();
    return LearnEvent::Success;
}

bool IrLearner::readCapture(IrCapture& out) {
    if (decodeResults.overflow) {
        return false;
    }
    const String protocol = typeToString(decodeResults.decode_type);
    strncpy(out.protocol, protocol.c_str(), sizeof(out.protocol) - 1);
    out.protocol[sizeof(out.protocol) - 1] = '\0';
    out.value = decodeResults.value;
    out.bits = decodeResults.bits;

    uint16_t* raw = resultToRawArray(&decodeResults);
    const uint16_t length = getCorrectedRawLength(&decodeResults);
    out.rawLength = length > IR_RAW_MAX_LENGTH ? IR_RAW_MAX_LENGTH : length;
    memcpy(out.raw, raw, out.rawLength * sizeof(uint16_t));
    delete[] raw;
    return out.rawLength > 0;
}

bool IrLearner::capturesMatch() const {
    const IrCapture& a = captures_[0];
    const IrCapture& b = captures_[1];
    const bool decoded = strcmp(a.protocol, "UNKNOWN") != 0;
    if (decoded) {
        return strcmp(a.protocol, b.protocol) == 0 && a.value == b.value && a.bits == b.bits;
    }
    return rawCapturesMatch(a.raw, a.rawLength, b.raw, b.rawLength, IR_MATCH_TOLERANCE_PERCENT,
                            IR_MATCH_MIN_TOLERANCE_US);
}

void IrLearner::setPower(bool on) {
    if (PIN_IR_RX_POWER < 0) {
        return;  // Mắt thu cấp nguồn thường trực từ 3V3
    }
    digitalWrite(PIN_IR_RX_POWER, on == IR_RX_POWER_ACTIVE_HIGH ? HIGH : LOW);
}
