#include "IrCompare.h"

bool rawCapturesMatch(const uint16_t* a, size_t lengthA, const uint16_t* b, size_t lengthB,
                      uint8_t tolerancePercent, uint16_t minToleranceUs) {
    if (a == nullptr || b == nullptr || lengthA == 0 || lengthA != lengthB) {
        return false;
    }
    for (size_t i = 0; i < lengthA; i++) {
        const uint32_t hi = a[i] > b[i] ? a[i] : b[i];
        const uint32_t lo = a[i] > b[i] ? b[i] : a[i];
        uint32_t tolerance = hi * tolerancePercent / 100;
        if (tolerance < minToleranceUs) {
            tolerance = minToleranceUs;
        }
        if (hi - lo > tolerance) {
            return false;
        }
    }
    return true;
}
