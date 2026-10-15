#include "BatteryFilter.h"

uint16_t medianMillivolts(uint16_t* samples, uint8_t count) {
    if (samples == nullptr || count == 0) {
        return 0;
    }
    // Sắp xếp chèn: chỉ vài chục mẫu
    for (uint8_t i = 1; i < count; i++) {
        const uint16_t value = samples[i];
        uint8_t j = i;
        for (; j > 0 && samples[j - 1] > value; j--) {
            samples[j] = samples[j - 1];
        }
        samples[j] = value;
    }
    if (count % 2 == 1) {
        return samples[count / 2];
    }
    return static_cast<uint16_t>((static_cast<uint32_t>(samples[count / 2 - 1]) + samples[count / 2]) / 2);
}

uint16_t BatteryFilter::add(uint16_t millivolts) {
    if (!initialized_) {
        scaled_ = static_cast<uint32_t>(millivolts) << shift_;
        initialized_ = true;
    } else {
        // scaled += x - scaled / 2^shift  <=>  value += (x - value) / 2^shift
        scaled_ = scaled_ - (scaled_ >> shift_) + millivolts;
    }
    return value();
}
