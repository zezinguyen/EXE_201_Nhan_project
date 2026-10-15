#include "Outbox.h"

#include <Arduino.h>
#include <ArduinoJson.h>

Outbox::Outbox(SupabaseRest& rest, ErrorReporter& errors) : rest_(rest), errors_(errors) {}

bool Outbox::queueLearnedCode(const IrCapture& capture, uint32_t epochSec) {
    LearnedCode item;
    item.epochSec = epochSec;
    item.capture = capture;
    return learnedCodes_.push(item);
}

void Outbox::update(uint32_t nowMs, bool online) {
    if (!online || !rest_.isReady()) {
        return;
    }
    if (waitingRetry_ && static_cast<int32_t>(nowMs - retryAtMs_) < 0) {
        return;
    }
    waitingRetry_ = false;

    bool ok = true;
    if (!learnedCodes_.empty()) {
        ok = sendLearnedCode(learnedCodes_.at(0));
        if (ok) {
            LearnedCode sent;
            learnedCodes_.pop(sent);
        }
    } else {
        ErrorRecord record;
        if (!errors_.peekPending(record)) {
            return;
        }
        ok = sendError(record);
        if (ok) {
            errors_.dropPending();
        }
    }

    if (!ok) {
        waitingRetry_ = true;
        retryAtMs_ = nowMs + OUTBOX_RETRY_MS;
    }
}

bool Outbox::sendLearnedCode(const LearnedCode& item) {
    JsonDocument row;
    row["home_id"] = rest_.homeId();
    row["board_id"] = rest_.boardId();
    if (item.epochSec > 0) {
        row["learned_at_epoch"] = item.epochSec;  // 0 = chưa có giờ NTP lúc học
    }
    irCaptureToJson(item.capture, row["ir"].to<JsonObject>());
    return rest_.insertRow(TABLE_LEARNED_CODES, row);
}

bool Outbox::sendError(const ErrorRecord& record) {
    JsonDocument row;
    row["home_id"] = rest_.homeId();
    row["board_id"] = rest_.boardId();
    row["code"] = errorCodeName(record.code);
    if (record.deviceId[0] != '\0') {
        row["device_id"] = record.deviceId;
    }
    row["occurred_at_epoch"] = record.epochSec;
    row["firmware"] = FIRMWARE_VERSION;
    return rest_.insertRow(TABLE_ERROR_REPORTS, row);
}
