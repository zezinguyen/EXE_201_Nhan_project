#pragma once
#include <stdint.h>

enum class BatteryLevel : uint8_t {
    Normal,
    Low,       // <= lowPercent: báo chủ nhà
    Critical,  // <= criticalPercent: tắt OLED/loa
    Empty,     // <= emptyPercent: báo sắp hết pin, chỉ giữ chức năng cơ bản
};

struct BatteryThresholds {
    uint8_t lowPercent;
    uint8_t criticalPercent;
    uint8_t emptyPercent;
};

// Quy đổi điện áp LiPo 1 cell -> % và phân mức có trễ (hysteresis)
// để không nhảy qua lại quanh ngưỡng.
class BatteryGauge {
public:
    BatteryGauge(BatteryThresholds thresholds, uint8_t hysteresisPercent);

    // Bảng tra gần đúng cho LiPo khi tải nhẹ. Cần hiệu chỉnh bằng đo thực tế.
    static uint8_t percentFromMillivolts(uint16_t millivolts);

    BatteryLevel update(uint8_t percent);
    BatteryLevel level() const { return level_; }

private:
    BatteryLevel levelFor(int16_t percent) const;

    BatteryThresholds thresholds_;
    uint8_t hysteresisPercent_;
    BatteryLevel level_ = BatteryLevel::Normal;
    bool initialized_ = false;
};
