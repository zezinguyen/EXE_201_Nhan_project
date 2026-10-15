#pragma once
#include <stdint.h>

// Khởi tạo mức board: Serial, bus I2C, ADC... – phần này khác nhau giữa các MCU.
// Triển khai: src/platform/<mcu>/Board.cpp
void boardBegin();

// Số ngẫu nhiên phần cứng (dùng cho jitter, mã ghép đôi).
uint32_t boardRandom();

// Khởi động lại MCU.
void boardRestart();
