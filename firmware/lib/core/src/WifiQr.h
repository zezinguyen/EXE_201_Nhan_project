#pragma once
#include <stddef.h>

// Nội dung mã QR WiFi (chuẩn điện thoại quét để vào WiFi): WIFI:T:WPA;S:<ssid>;P:<mật khẩu>;;
// Ký tự đặc biệt \ ; , : " trong ssid/mật khẩu được thêm \ phía trước.
// Trả về false nếu tham số null hoặc out không đủ chỗ.
bool buildWifiQrPayload(char* out, size_t outSize, const char* ssid, const char* password);
