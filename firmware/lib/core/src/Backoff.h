#pragma once
#include <stdint.h>

// Khoảng chờ kết nối lại tăng dần (x2 mỗi lần) có ngẫu nhiên để nhiều mạch
// không cùng thử lại một lúc. VD 2s -> 4s -> 8s ... -> max.
class Backoff {
public:
    Backoff(uint32_t baseMs, uint32_t maxMs, uint8_t jitterPercent);

    // Trả về khoảng chờ cho lần thử tiếp theo rồi tăng mức chờ.
    // randomValue: số ngẫu nhiên bất kỳ (truyền vào để test được).
    uint32_t nextDelayMs(uint32_t randomValue);
    void reset();
    uint16_t attempts() const { return attempts_; }

private:
    uint32_t baseMs_;
    uint32_t maxMs_;
    uint32_t currentMs_;
    uint8_t jitterPercent_;
    uint16_t attempts_ = 0;
};
