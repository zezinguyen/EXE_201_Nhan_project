#pragma once
#include <stddef.h>
#include <stdint.h>

// So sánh 2 lần thu IR dạng raw (µs). Khớp khi cùng độ dài và từng khoảng
// lệch không quá max(tolerancePercent% của giá trị lớn hơn, minToleranceUs).
bool rawCapturesMatch(const uint16_t* a, size_t lengthA, const uint16_t* b, size_t lengthB,
                      uint8_t tolerancePercent, uint16_t minToleranceUs);
