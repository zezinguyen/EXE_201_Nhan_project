#include "ErrorCodes.h"

#include <string.h>

const char* errorCodeName(ErrorCode code) {
    switch (code) {
        case ErrorCode::ProfileInvalid: return "E_PROFILE_INVALID";
        case ErrorCode::CmdNotFound: return "E_CMD_NOT_FOUND";
        case ErrorCode::IrSendFail: return "E_IR_SEND_FAIL";
        case ErrorCode::UserReported: return "E_USER_REPORTED";
        case ErrorCode::LearnFail: return "E_LEARN_FAIL";
        case ErrorCode::LowBattery: return "E_LOW_BATTERY";
    }
    return "E_UNKNOWN";
}

bool errorCodeFromName(const char* name, ErrorCode& out) {
    if (name == nullptr) {
        return false;
    }
    for (uint8_t i = 0; i <= static_cast<uint8_t>(ErrorCode::LowBattery); i++) {
        const ErrorCode code = static_cast<ErrorCode>(i);
        if (strcmp(name, errorCodeName(code)) == 0) {
            out = code;
            return true;
        }
    }
    return false;
}
