#pragma once

// Vẽ mã QR: gọi plot(ctx, size, x, y) cho từng ô đen (size = số ô mỗi cạnh, để căn giữa). Triển khai theo MCU: src/platform/<mcu>/QrCode.cpp
// (ESP32 dùng esp_qrcode có sẵn trong SDK).
using QrPlotFn = void (*)(void* ctx, int size, int x, int y);

// Trả về số ô mỗi cạnh (version 1 = 21, 2 = 25, 3 = 29), 0 nếu text không vừa maxVersion.
int renderQrCode(const char* text, int maxVersion, QrPlotFn plot, void* ctx);
