#include "WifiQr.h"

#include <string.h>

namespace {
// Ghi text vào out[pos..], thêm \ trước ký tự đặc biệt nếu escape. false nếu hết chỗ (chừa '\0').
bool append(char* out, size_t outSize, size_t& pos, const char* text, bool escape) {
    for (const char* c = text; *c != '\0'; c++) {
        if (escape && strchr("\\;,:\"", *c) != nullptr) {
            if (pos + 1 >= outSize) {
                return false;
            }
            out[pos++] = '\\';
        }
        if (pos + 1 >= outSize) {
            return false;
        }
        out[pos++] = *c;
    }
    return true;
}
}  // namespace

bool buildWifiQrPayload(char* out, size_t outSize, const char* ssid, const char* password) {
    if (out == nullptr || outSize == 0 || ssid == nullptr || password == nullptr) {
        return false;
    }
    size_t pos = 0;
    const bool ok = append(out, outSize, pos, "WIFI:T:WPA;S:", false) && append(out, outSize, pos, ssid, true) &&
                    append(out, outSize, pos, ";P:", false) && append(out, outSize, pos, password, true) &&
                    append(out, outSize, pos, ";;", false);
    out[ok ? pos : 0] = '\0';
    return ok;
}
