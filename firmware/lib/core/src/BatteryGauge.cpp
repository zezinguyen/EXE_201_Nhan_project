#include "BatteryGauge.h"

namespace {
struct CurvePoint {
    uint16_t millivolts;
    uint8_t percent;
};

// Giảm dần theo điện áp
constexpr CurvePoint kCurve[] = {
    {4200, 100}, {4100, 90}, {4000, 78}, {3900, 63}, {3800, 45}, {3750, 32},
    {3700, 20},  {3650, 12}, {3600, 7},  {3500, 3},  {3300, 0},
};
constexpr uint8_t kCurveSize = sizeof(kCurve) / sizeof(kCurve[0]);
}  // namespace

BatteryGauge::BatteryGauge(BatteryThresholds thresholds, uint8_t hysteresisPercent)
    : thresholds_(thresholds), hysteresisPercent_(hysteresisPercent) {}

uint8_t BatteryGauge::percentFromMillivolts(uint16_t millivolts) {
    if (millivolts >= kCurve[0].millivolts) {
        return 100;
    }
    if (millivolts <= kCurve[kCurveSize - 1].millivolts) {
        return 0;
    }
    for (uint8_t i = 1; i < kCurveSize; i++) {
        const CurvePoint& hi = kCurve[i - 1];
        const CurvePoint& lo = kCurve[i];
        if (millivolts >= lo.millivolts) {
            const uint32_t span = hi.millivolts - lo.millivolts;
            const uint32_t offset = millivolts - lo.millivolts;
            return static_cast<uint8_t>(lo.percent + (hi.percent - lo.percent) * offset / span);
        }
    }
    return 0;
}

BatteryLevel BatteryGauge::update(uint8_t percent) {
    const BatteryLevel raw = levelFor(percent);
    if (!initialized_) {
        level_ = raw;
        initialized_ = true;
        return level_;
    }

    if (raw > level_) {
        // Xấu đi: cập nhật ngay
        level_ = raw;
    } else if (raw < level_) {
        // Tốt lên: phải vượt ngưỡng thêm hysteresis mới đổi
        const BatteryLevel withMargin = levelFor(static_cast<int16_t>(percent) - hysteresisPercent_);
        if (withMargin < level_) {
            level_ = withMargin;
        }
    }
    return level_;
}

BatteryLevel BatteryGauge::levelFor(int16_t percent) const {
    if (percent <= thresholds_.emptyPercent) {
        return BatteryLevel::Empty;
    }
    if (percent <= thresholds_.criticalPercent) {
        return BatteryLevel::Critical;
    }
    if (percent <= thresholds_.lowPercent) {
        return BatteryLevel::Low;
    }
    return BatteryLevel::Normal;
}
