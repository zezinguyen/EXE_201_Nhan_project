#pragma once
#include <stdint.h>

// Trung vị của một loạt mẫu ADC (sắp xếp tại chỗ): bỏ các mẫu lệch do WiFi phát / nhiễu.
uint16_t medianMillivolts(uint16_t* samples, uint8_t count);

// Làm mượt điện áp pin theo thời gian (trung bình trượt hàm mũ, hệ số 1/2^smoothingShift).
// Điện áp đo lúc có tải nhảy vài chục mV giữa các lần đo; ở đoạn 3.8–3.9V của LiPo
// 70mV đã là ~15%, nên phải lọc trước khi quy ra %.
class BatteryFilter {
public:
    explicit BatteryFilter(uint8_t smoothingShift) : shift_(smoothingShift) {}

    // Thêm một lần đo, trả về giá trị đã lọc. Lần đầu lấy luôn giá trị đo.
    uint16_t add(uint16_t millivolts);
    uint16_t value() const { return static_cast<uint16_t>(scaled_ >> shift_); }
    // Bỏ lịch sử (VD vừa cắm/rút sạc: điện áp đổi bậc, không nên trôi từ từ)
    void reset() { initialized_ = false; }

private:
    uint8_t shift_;
    uint32_t scaled_ = 0;  // value * 2^shift_
    bool initialized_ = false;
};
