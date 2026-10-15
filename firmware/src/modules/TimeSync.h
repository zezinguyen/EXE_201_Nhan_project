#pragma once
#include <stdint.h>

// Đồng bộ giờ NTP để kiểm tra hạn dùng lệnh (spec 4.1).
// Triển khai riêng theo MCU: src/platform/<mcu>/TimeSync.cpp
class TimeSync {
public:
    void begin();
    bool isSynced() const;
    int64_t nowEpochMs() const;
    uint32_t nowEpochSec() const { return static_cast<uint32_t>(nowEpochMs() / 1000); }
};
