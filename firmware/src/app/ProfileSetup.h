#pragma once
#include <ArduinoJson.h>

#include "app/CommandHandler.h"
#include "modules/DeviceStore.h"
#include "modules/IrSender.h"
#include "modules/RealtimeClient.h"

// Sửa profile trên mạch từ app khi server chưa quản lý profile (TẠM — contracts.md mục 3.8):
//   profile_get          -> profile_info: danh sách thiết bị + giao thức đang dùng + giao thức gợi ý
//   ac_try               -> setup_result: phát thử một giao thức điều hòa (không lưu)
//   ac_set_protocol      -> setup_result: lưu giao thức điều hòa (tạo điều hòa nếu chưa có)
//   ir_try               -> setup_result: phát thử một IrCode (VD mã Tivi theo hãng), không lưu
//   profile_put_command  -> setup_result: lưu một phím đã học cho thiết bị bấm nút (VD Tivi)
// Profile lưu trên LittleFS. Khi có Edge Function + profile_updated (mục 3.7) thì bỏ module này.
class ProfileSetup {
public:
    ProfileSetup(DeviceStore& store, IrSender& sender, CommandHandler& commands, RealtimeClient& realtime);

    // true nếu event thuộc module này (đã xử lý), false để chuyển cho CommandHandler
    bool onBroadcast(const char* event, JsonObjectConst payload);

private:
    void sendProfileInfo(const char* requestId);
    const char* tryProtocol(JsonObjectConst payload);
    const char* tryIrCode(JsonObjectConst payload);
    const char* setProtocol(JsonObjectConst payload);
    const char* putCommand(JsonObjectConst payload);
    void sendResult(const char* requestId, const char* error);

    DeviceStore& store_;
    IrSender& sender_;
    CommandHandler& commands_;
    RealtimeClient& realtime_;
};
