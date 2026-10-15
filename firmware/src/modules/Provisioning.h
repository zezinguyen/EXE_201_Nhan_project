#pragma once
#include <stdint.h>

#include "SecureStore.h"

// Ghép đôi mạch với nhà (spec 10.2): mã 6 số hiện trên OLED, dùng 1 lần,
// hết hạn sau PAIRING_CODE_TTL_MS. Việc xác minh mã do Edge Function làm.
class Provisioning {
public:
    void begin(SecureStore& store);
    void update(uint32_t nowMs);

    bool isPaired() const;
    void startPairing(uint32_t nowMs);
    void cancelPairing();
    bool isPairing() const { return pairing_; }
    const char* pairingCode() const { return pairing_ ? code_ : nullptr; }

    // Gọi khi Edge Function xác nhận ghép đôi thành công và trả về token riêng của mạch.
    // TODO(tuần 5): đăng ký chờ ghép (gửi boardId + mã) và nhận token từ Edge Function
    bool completePairing(const char* boardToken, const char* boardId, const char* homeId);

    // Xoá token + WiFi (giữ Nút 3 rất lâu). Nhà cũ cũng phải gỡ mạch trên app.
    void factoryReset();

private:
    SecureStore* store_ = nullptr;
    bool pairing_ = false;
    uint32_t startMs_ = 0;
    char code_[7] = {};
};
