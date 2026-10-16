#include "DisplayOled.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Elapsed.h>
#include <WifiQr.h>
#include <Wire.h>
#include <string.h>

#include "QrCode.h"
#include "config.h"

namespace {
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

constexpr int kCharWidth = 6;    // Chữ cỡ 1: 6x8px (cỡ 2: 12x16px)
constexpr int kQrPayloadMax = 96;
// Ô nền trắng của mã QR (version lớn nhất), sát mép trái
constexpr int kQrBox = OLED_QR_MAX_VERSION * 4 + 17;  // Số ô mỗi cạnh của version lớn nhất
constexpr int kQrBoxPx = kQrBox * OLED_QR_SCALE + 2 * OLED_QR_QUIET_PX;
constexpr int kQrBoxY = (OLED_HEIGHT - kQrBoxPx) / 2;
static_assert(kQrBoxPx <= OLED_HEIGHT, "Ma QR khong vua chieu cao OLED");

// Ô đen của QR, căn giữa trong ô nền trắng (version nhỏ hơn thì có thêm viền)
void plotQrModule(void* /*ctx*/, int size, int x, int y) {
    const int offset = (kQrBoxPx - size * OLED_QR_SCALE) / 2;
    oled.fillRect(offset + x * OLED_QR_SCALE, kQrBoxY + offset + y * OLED_QR_SCALE, OLED_QR_SCALE, OLED_QR_SCALE,
                  SSD1306_BLACK);
}

void printAt(int x, int y, uint8_t size, const char* text) {
    oled.setTextSize(size);
    oled.setCursor(x, y);
    oled.print(text);
    oled.setTextSize(1);
}

// In chữ tự xuống dòng theo từ (từ dài hơn một dòng thì cắt theo ký tự), tối đa maxLines dòng
void printWrapped(int y, uint8_t size, const char* text, int maxLines) {
    const int perLine = OLED_WIDTH / (kCharWidth * size);
    const int lineHeight = 8 * size;
    oled.setTextSize(size);
    const char* p = text;
    for (int line = 0; line < maxLines && *p != '\0'; line++) {
        while (*p == ' ') {
            p++;
        }
        int take = static_cast<int>(strnlen(p, perLine + 1));
        if (take > perLine) {
            take = perLine;
            for (int i = perLine; i > 0; i--) {  // Lùi về dấu cách gần nhất để không cắt giữa từ
                if (p[i] == ' ') {
                    take = i;
                    break;
                }
            }
        }
        oled.setCursor(0, y + line * lineHeight);
        oled.write(reinterpret_cast<const uint8_t*>(p), take);
        p += take;
    }
    oled.setTextSize(1);
}

// Mã QR WiFi bên trái, chữ hướng dẫn + mã ghép đôi bên phải. false nếu không tạo được QR.
bool drawSetupQr(const DisplayStatus& status) {
    char payload[kQrPayloadMax];
    if (!buildWifiQrPayload(payload, sizeof(payload), status.setupSsid, status.setupPassword)) {
        return false;
    }
    oled.fillRect(0, kQrBoxY, kQrBoxPx, kQrBoxPx, SSD1306_WHITE);
    if (renderQrCode(payload, OLED_QR_MAX_VERSION, plotQrModule, nullptr) == 0) {
        return false;
    }
    const int x = kQrBoxPx + 4;
    printAt(x, 2, 1, "Quet QR");
    printAt(x, 12, 1, "vao WiFi");
    if (status.pairingCode != nullptr) {
        printAt(x, 28, 1, "Ma ghep:");
        printAt(x, 38, 1, status.pairingCode);
    }
    printAt(x, 56, 1, "Nut: chu");
    return true;
}

// Chữ to: tên WiFi, mật khẩu cỡ 2, mã ghép đôi cỡ 2
void drawSetupText(const DisplayStatus& status) {
    oled.setCursor(0, 11);
    oled.printf("WiFi %s", status.setupSsid);
    printAt(0, 21, 2, status.setupPassword);  // 10 ký tự x 12px = 120px
    if (status.pairingCode != nullptr) {
        printAt(0, 46, 1, "Ma");
        printAt(16, 42, 2, status.pairingCode);
        printAt(OLED_WIDTH - 4 * kCharWidth, 42, 1, "Nut:");
        printAt(OLED_WIDTH - 3 * kCharWidth, 52, 1, " QR");
    } else if (status.message != nullptr) {
        printAt(0, 44, 1, status.message);
    }
}
}  // namespace

bool DisplayOled::begin() {
    ready_ = oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS);
    if (ready_) {
        oled.clearDisplay();
        oled.setTextColor(SSD1306_WHITE);
        oled.display();
    }
    return ready_;
}

void DisplayOled::wake(uint32_t nowMs) {
    if (!ready_ || disabled_) {
        return;
    }
    wakeMs_ = nowMs;
    if (!awake_) {
        oled.ssd1306_command(SSD1306_DISPLAYON);
        awake_ = true;
    }
}

void DisplayOled::sleep() {
    if (ready_ && awake_) {
        oled.ssd1306_command(SSD1306_DISPLAYOFF);
    }
    awake_ = false;
}

void DisplayOled::update(uint32_t nowMs) {
    if (awake_ && elapsedMs(nowMs, wakeMs_) >= OLED_TIMEOUT_MS) {
        sleep();
    }
}

void DisplayOled::show(const DisplayStatus& status) {
    if (!ready_ || !awake_) {
        return;
    }
    oled.clearDisplay();
    oled.setTextSize(1);

    // Chế độ cài đặt dạng QR: dùng cả màn hình, không có dòng trạng thái
    if (status.setupSsid != nullptr && status.setupShowQr && drawSetupQr(status)) {
        oled.display();
        return;
    }
    oled.clearDisplay();  // QR lỗi giữa chừng: xoá phần đã vẽ, hiện dạng chữ

    oled.setCursor(0, 0);
    oled.print(connectionStateText(status.connection));

    // Góc phải: "Pin 62%" hoặc "Sac 62%" khi đang sạc (chỉ khi cắm cổng sạc của module sạc,
    // cắm USB vào board MCU không sạc pin). Căn phải, chữ 6px/ký tự.
    char battery[12];
    const int length = snprintf(battery, sizeof(battery), "%s %u%%", status.charging ? "Sac" : "Pin",
                                status.batteryPercent);
    oled.setCursor(OLED_WIDTH - length * kCharWidth, 0);
    oled.print(battery);

    if (status.setupSsid != nullptr) {
        drawSetupText(status);
    } else if (status.pairingCode != nullptr) {
        printAt(0, 30, 1, "Ma:");
        printAt(24, 26, 2, status.pairingCode);
    } else if (status.message != nullptr) {
        printWrapped(16, 2, status.message, 3);  // Cỡ 2: 10 ký tự/dòng, 3 dòng
    } else {
        printWrapped(24, 2, connectionStateText(status.connection), 2);
    }
    oled.display();
}

void DisplayOled::setDisabled(bool disabled) {
    disabled_ = disabled;
    if (disabled_) {
        sleep();
    }
}
