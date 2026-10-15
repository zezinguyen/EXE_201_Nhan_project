#pragma once
#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

// Bản sao profile của nhà (thiết bị, mã IR, remote ảo) – spec 4.2, 6.
// Phần tra cứu dùng chung: src/modules/DeviceStore.cpp
// Phần đọc/ghi file riêng theo MCU: src/platform/<mcu>/DeviceStore.cpp
//
// Định dạng: xem data/profile.json
class DeviceStore {
public:
    bool begin();                                // Mount filesystem
    bool load();                                 // Đọc profile (hỏng thì thử bản sao lưu)
    bool save(const char* json, size_t length);  // Kiểm tra hợp lệ, ghi file tạm, giữ bản cũ
    bool loadFromJson(const char* json, size_t length);  // Chỉ nạp vào RAM (test, debug)

    uint32_t profileVersion() const;
    // Toàn bộ profile đang nạp (đọc, hoặc chép ra để sửa rồi save())
    const JsonDocument& document() const { return doc_; }
    JsonObjectConst findDevice(const char* deviceId) const;
    JsonObjectConst findCommand(const char* deviceId, const char* action) const;

private:
    static bool isValidProfile(const JsonDocument& doc);

    JsonDocument doc_;
};
