#pragma once
#include <ArduinoJson.h>
#include <stddef.h>

// Sửa profile (định dạng docs/contracts.md mục 5) ngay trên mạch: bản thử (web) và app/ProfileSetup (TẠM,
// contracts.md mục 3.8). Khi server quản lý profile thì mạch nhận profile từ Supabase, không tự sửa.
// Các hàm sửa trả về true khi profile đã đổi; khi đó profileVersion tăng 1.

// Tên phím/thiết bị: 1..maxBytes byte, không có ký tự điều khiển (UTF-8 được phép).
bool isValidProfileName(const char* name, size_t maxBytes);

// Thêm điều hòa nếu chưa có deviceId này (không sửa thiết bị đã có).
bool profileEnsureAc(JsonDocument& doc, const char* deviceId, const char* name, const char* protocol);

// Đổi giao thức IRac của điều hòa đã có. Sai deviceId hoặc không phải điều hòa -> false.
bool profileSetAcProtocol(JsonDocument& doc, const char* deviceId, const char* protocol);

// Thêm/ghi đè phím `action` (IrCode: {protocol, code, bits} hoặc {raw, freq}) cho thiết bị bấm nút,
// tạo thiết bị (type "custom") nếu chưa có. Từ chối điều hòa: điều hòa gửi cả trạng thái qua IRac.
bool profilePutCommand(JsonDocument& doc, const char* deviceId, const char* deviceName, const char* action,
                       JsonObjectConst ir);

bool profileRemoveCommand(JsonDocument& doc, const char* deviceId, const char* action);
