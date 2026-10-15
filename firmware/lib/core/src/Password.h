#pragma once
#include <stddef.h>
#include <stdint.h>

using RandomFn = uint32_t (*)();

// Bảng ký tự đã bỏ các ký tự dễ nhầm khi đọc trên OLED: 0/o, 1/l/i
constexpr const char* kPasswordAlphabet = "abcdefghjkmnpqrstuvwxyz23456789";
constexpr size_t kMinWpaPasswordLength = 8;

// Sinh mật khẩu ngẫu nhiên dài length ký tự (+ '\0') vào out.
// Trả về false nếu length < 8 (tối thiểu của WPA2) hoặc out không đủ chỗ.
bool generatePassword(char* out, size_t outSize, size_t length, RandomFn randomFn);
