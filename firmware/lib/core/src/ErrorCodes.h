#pragma once
#include <stdint.h>

// Mã lỗi theo spec mục 8
enum class ErrorCode : uint8_t {
    ProfileInvalid,
    CmdNotFound,
    IrSendFail,
    UserReported,
    LearnFail,
    LowBattery,
};

const char* errorCodeName(ErrorCode code);
// Ngược lại của errorCodeName. Trả về false nếu tên không phải mã lỗi spec.
bool errorCodeFromName(const char* name, ErrorCode& out);
