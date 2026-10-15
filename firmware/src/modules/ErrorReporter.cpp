#include "ErrorReporter.h"

#include <Arduino.h>
#include <string.h>

ErrorReporter::ErrorReporter() : throttle_(ERROR_REPEAT_THRESHOLD, ERROR_REPEAT_WINDOW_SEC, ERROR_MIN_RESEND_SEC) {}

void ErrorReporter::report(ErrorCode code, const char* deviceId, uint32_t nowSec, bool userReported) {
    log_w("Loi %s thiet bi %s", errorCodeName(code), deviceId != nullptr ? deviceId : "-");
    if (!throttle_.shouldReport(code, hashKey(deviceId), nowSec, userReported)) {
        return;
    }
    ErrorRecord record = {};
    record.epochSec = nowSec;
    record.code = code;
    if (deviceId != nullptr) {
        strncpy(record.deviceId, deviceId, sizeof(record.deviceId) - 1);
    }
    queue_.push(record);
}

void ErrorReporter::dropPending() {
    ErrorRecord ignored;
    queue_.pop(ignored);
}
