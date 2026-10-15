#pragma once
#include <ArduinoJson.h>
#include <stddef.h>

// Ghi bảng Supabase qua REST (PostgREST) bằng token riêng của mạch; RLS kiểm tra quyền.
// Dùng cho dữ liệu cần lưu lại kể cả khi app không mở (mã học được, báo lỗi).
// Gọi đồng bộ (chặn tới REST_TIMEOUT_MS) – chỉ dùng cho việc hiếm, không dùng cho lệnh.
// Triển khai riêng theo MCU: src/platform/<mcu>/SupabaseRest.cpp
class SupabaseRest {
public:
    void begin(const char* boardToken, const char* homeId, const char* boardId);
    void end();
    bool isReady() const { return ready_; }
    const char* homeId() const { return homeId_; }
    const char* boardId() const { return boardId_; }

    bool insertRow(const char* table, const JsonDocument& row);

private:
    bool ready_ = false;
    char token_[512] = {};
    char homeId_[64] = {};
    char boardId_[64] = {};
};
