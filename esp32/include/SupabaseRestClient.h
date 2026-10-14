#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <vector>
#include "config.h"
#include "IrCode.h"

struct QueuedErrorReport {
    String code;
    String deviceId;
    uint32_t occurredEpoch;
};

class SupabaseRestClient {
public:
    static SupabaseRestClient& instance() {
        static SupabaseRestClient client;
        return client;
    }

    void begin();
    void update();

    // Gọi các RPC theo contracts.md
    bool sendAck(const String& commandId, bool ok, const String& error = "", bool duplicate = false);
    bool sendLearnEvent(const String& event, const String& requestId, const String& status, const IrCode* ir = nullptr);
    bool sendStatusReport();

    // Hàng đợi gửi bù
    void queueLearnedCode(const IrCode& code);
    void queueErrorReport(const String& code, const String& deviceId = "");

private:
    SupabaseRestClient() = default;

    std::vector<IrCode> pendingLearnedCodes;
    std::vector<QueuedErrorReport> pendingErrorReports;

    unsigned long lastStatusReportTime = 0;
    unsigned long lastQueueDrainTime = 0;

    bool postRpc(const String& rpcName, const String& jsonBody);
    bool postRest(const String& table, const String& jsonBody);
    void drainQueues();
};

