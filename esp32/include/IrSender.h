#pragma once
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRac.h>
#include <vector>
#include "config.h"
#include "IrCode.h"

struct SendResult {
    bool ok = false;
    String error = "";
    bool duplicate = false;
};

class IrSender {
public:
    static IrSender& instance() {
        static IrSender sender;
        return sender;
    }

    void begin();

    // Xử lý gửi lệnh
    SendResult handleCommand(
        const String& commandId,
        const String& deviceId,
        const String& action,
        const JsonObjectConst& params,
        uint64_t expiresAt,
        uint32_t seq
    );

private:
    IrSender();

    IRsend irSend;
    IRac irAc;

    // Cache 16 commandId gần nhất chống lặp
    std::vector<String> recentCommandIds;
    // Đếm giới hạn tốc độ 5 lệnh/giây
    uint32_t commandTimestamps[MAX_COMMANDS_PER_SECOND] = {0};
    uint8_t rateIndex = 0;
    uint32_t lastSeenSeq = 0;

    bool isRateLimited();
    bool isDuplicate(const String& cmdId);
    void recordCommand(const String& cmdId);

    bool sendIrByCode(const IrCode& ir);
    bool sendAcCommand(const String& protocolStr, const JsonObjectConst& params, int tempMin, int tempMax);
};

