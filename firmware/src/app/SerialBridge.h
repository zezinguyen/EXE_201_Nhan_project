#pragma once
#include <ArduinoJson.h>
#include <stddef.h>

#include "config.h"
#include "modules/RealtimeClient.h"

// Đọc từng dòng JSON từ Serial và chuyển vào như một broadcast từ app, để thử
// mạch khi chưa có app/Supabase. Mỗi dòng có dạng:
//   {"event":"command","payload":{"commandId":"t1","deviceId":"tv_living","action":"POWER","seq":1}}
//   {"event":"learn_start","payload":{"requestId":"r1"}}
// Kết quả xem ở log (dòng "ACK ...").
class SerialBridge {
public:
    void begin(BroadcastHandler handler) { handler_ = handler; }
    void update();

private:
    void handleLine();

    BroadcastHandler handler_ = nullptr;
    char line_[SERIAL_BRIDGE_MAX_LINE] = {};
    size_t length_ = 0;
    bool overflow_ = false;
};
