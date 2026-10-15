#pragma once
#include <stdint.h>

enum class PressType : uint8_t {
    None,
    Short,  // Nhả trước mốc giữ đầu tiên
    Hold,   // Nhả sau khi đã giữ qua ít nhất một mốc (xem holdLevel)
};

struct ButtonEvent {
    PressType type;
    uint8_t holdLevel;  // 1..số mốc khi type == Hold, ngược lại 0
};

// Chống dội + phân loại bấm ngắn / giữ theo nhiều mốc (VD 5s, 10s, 20s).
// Sự kiện phát ra khi NHẢ nút: giữ tới mốc nào thì nhận mốc đó, để giữ 20s
// không kích hoạt nhầm hành động của mốc 5s.
class ButtonDetector {
public:
    static constexpr uint8_t kMaxHoldLevels = 4;

    // holdLevelsMs: các mốc tăng dần. Thừa quá kMaxHoldLevels thì bỏ.
    ButtonDetector(uint16_t debounceMs, const uint32_t* holdLevelsMs, uint8_t levelCount);

    ButtonEvent update(bool rawPressed, uint32_t nowMs);
    bool isPressed() const { return stablePressed_; }
    // Thời gian đang giữ (0 nếu không giữ)
    uint32_t heldMs(uint32_t nowMs) const;
    // Mốc đã đạt nếu nhả tay lúc này (0 = bấm ngắn hoặc không giữ) – để OLED gợi ý
    uint8_t currentHoldLevel(uint32_t nowMs) const;

private:
    uint8_t holdLevelFor(uint32_t heldMs) const;

    uint32_t holdLevelsMs_[kMaxHoldLevels] = {};
    uint8_t levelCount_ = 0;
    uint16_t debounceMs_;
    bool lastRaw_ = false;
    bool stablePressed_ = false;
    uint32_t lastChangeMs_ = 0;
    uint32_t pressStartMs_ = 0;
};
