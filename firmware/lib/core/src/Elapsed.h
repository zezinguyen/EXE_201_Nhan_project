#pragma once
#include <stdint.h>

// Thời gian đã trôi qua từ startMs tới nowMs, an toàn khi millis() tràn số.
// Vòng lặp lấy nowMs ở đầu vòng, nhưng một sự kiện giữa vòng (nút, web) có thể đặt
// startMs = millis() MUỘN hơn nowMs: phép trừ không dấu khi đó quay vòng thành số rất lớn
// -> hết giờ ngay lập tức. Hàm này coi trường hợp đó là 0ms (mốc chỉ hợp lệ trong ~24 ngày).
inline uint32_t elapsedMs(uint32_t nowMs, uint32_t startMs) {
    const int32_t diff = static_cast<int32_t>(nowMs - startMs);
    return diff > 0 ? static_cast<uint32_t>(diff) : 0;
}
