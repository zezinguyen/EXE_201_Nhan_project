#pragma once
// =============================================================================
// config.h – NƠI DUY NHẤT đặt thông số và chân GPIO.
// Không viết số "magic" (chân, thời gian, ngưỡng) trong module; thêm vào đây.
// =============================================================================

#include <stddef.h>
#include <stdint.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#define USING_EXAMPLE_SECRETS 1  // main.cpp sẽ cảnh báo
#include "secrets.example.h"
#endif

#define FIRMWARE_VERSION "0.1.0"

// -----------------------------------------------------------------------------
// Chân GPIO theo board (chọn bằng -DBOARD_xxx trong platformio.ini)
// -----------------------------------------------------------------------------
#if defined(BOARD_ESP32_DEVKIT)

// DevKit 30 chân. Chia theo cạnh board để dây gọn (xem docs/specs.md mục 3.3):
//   Cạnh phải: OLED (trên), mắt thu IR, DFPlayer – các module đặt bên phải
//   Cạnh trái: đo pin/sạc (trên, gần TP4056), LED RGB, LED IR, nút (cạnh chân GND)
// Tránh chân strapping 0, 2, 5, 12, 15 và chân flash 6–11.

// --- Cạnh phải ---
constexpr uint8_t PIN_OLED_SCL = 22;
constexpr uint8_t PIN_OLED_SDA = 21;
constexpr int8_t PIN_IR_RX_POWER = -1;     // Chân cấp nguồn mắt thu; -1 = VCC mắt thu nối thẳng 3V3
constexpr uint8_t PIN_IR_RX = 18;          // OUT mắt thu
constexpr uint8_t PIN_AUDIO_TX = 17;       // TX2 -> DFPlayer RX (cùng mức 3.3V, nối thẳng)
constexpr uint8_t PIN_AUDIO_RX = 16;       // RX2 <- DFPlayer TX
constexpr uint8_t PIN_IR_TX = 14;          // Trở 330Ω -> chân B của S8050

// --- Cạnh trái ---
constexpr uint8_t PIN_BATTERY_ADC = 36;    // VP: cầu chia 100k/100k từ B+ (ADC1, chỉ vào)
constexpr uint8_t PIN_USB_SENSE = 39;      // VN: cầu chia 100k/100k từ IN+ của TP4056
constexpr uint8_t PIN_LED_RED = 27;        // LED RGB cathode chung: kéo lên = sáng
constexpr uint8_t PIN_LED_GREEN = 26;
constexpr uint8_t PIN_LED_BLUE = 25;
constexpr uint8_t PIN_BUTTON = 15;         // Nút -> chân GND ngay bên dưới

constexpr uint8_t AUDIO_UART_NUM = 2;      // UART phần cứng nối DFPlayer

#else
#error "Chua chon board: them -DBOARD_xxx vao build_flags va khai bao chan o day"
#endif

// Mức tích cực của phần cứng
constexpr bool BUTTON_ACTIVE_LOW = true;       // Nút nối GND, dùng pull-up trong
constexpr bool STATUS_LED_ACTIVE_HIGH = true;   // LED RGB cathode chung (chân chung về GND)
constexpr bool IR_RX_POWER_ACTIVE_HIGH = true;

// -----------------------------------------------------------------------------
// Pin / nguồn
// -----------------------------------------------------------------------------
constexpr float BATTERY_DIVIDER_RATIO = 2.0f;  // Cầu chia 2 x 100k
constexpr uint16_t USB_SENSE_MIN_MV = 1500;     // Đo ở chân VN: >= ngưỡng = đang cắm USB (5V/2 = 2.5V)
// Module TP4056 không đưa chân CHRG ra: cắm USB mà pin dưới ngưỡng này thì coi là đang sạc.
// Khi đang sạc, điện áp đo được cao hơn điện áp nghỉ của pin -> % hiển thị lúc sạc chỉ tương đối.
constexpr uint16_t BATTERY_FULL_MV = 4150;
constexpr uint8_t BATTERY_SAMPLES = 8;          // Số mẫu ADC lấy trung bình
constexpr uint32_t BATTERY_SAMPLE_INTERVAL_MS = 30UL * 1000;
constexpr uint8_t BATTERY_LOW_PERCENT = 20;      // Báo chủ nhà
constexpr uint8_t BATTERY_CRITICAL_PERCENT = 10; // Tắt OLED/loa
constexpr uint8_t BATTERY_EMPTY_PERCENT = 5;     // Báo "sắp hết pin", chỉ giữ chức năng cơ bản
constexpr uint8_t BATTERY_HYSTERESIS_PERCENT = 2;

constexpr uint32_t CPU_FREQ_BATTERY_MHZ = 80;   // Không hạ dưới 80 khi còn dùng WiFi
constexpr uint32_t CPU_FREQ_USB_MHZ = 240;
constexpr bool WIFI_USE_MAX_MODEM_SLEEP = false; // true = WIFI_PS_MAX_MODEM khi chạy pin

// -----------------------------------------------------------------------------
// WiFi / kết nối lại
// -----------------------------------------------------------------------------
constexpr uint8_t WIFI_MAX_NETWORKS = 3;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15UL * 1000;
constexpr uint32_t WIFI_SETUP_AP_AFTER_MS = 5UL * 60 * 1000; // Mất WiFi lâu -> bật AP cài đặt
constexpr const char* WIFI_SETUP_AP_PREFIX = "SmartHome-";
constexpr uint8_t WIFI_SETUP_AP_PASSWORD_LENGTH = 10; // Sinh ngẫu nhiên mỗi lần, hiện trên OLED

constexpr uint32_t RECONNECT_BASE_MS = 2000;   // 2s -> 4s -> 8s ... -> 60s
constexpr uint32_t RECONNECT_MAX_MS = 60UL * 1000;
constexpr uint8_t RECONNECT_JITTER_PERCENT = 20;

// -----------------------------------------------------------------------------
// Supabase Realtime
// -----------------------------------------------------------------------------
constexpr uint16_t SUPABASE_PORT = 443;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 25UL * 1000;
constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 10UL * 1000;
constexpr uint32_t STATUS_REPORT_INTERVAL_MS = 5UL * 60 * 1000; // Gửi pin/RSSI lên Presence
constexpr uint32_t REST_TIMEOUT_MS = 8000;                       // Ghi bảng qua REST (HTTPS)
constexpr uint32_t OUTBOX_RETRY_MS = 30UL * 1000;                // Gửi lỗi -> chờ rồi thử lại
constexpr const char* TABLE_LEARNED_CODES = "learned_codes";
constexpr const char* TABLE_ERROR_REPORTS = "error_reports";

// -----------------------------------------------------------------------------
// Lệnh
// -----------------------------------------------------------------------------
constexpr uint8_t COMMAND_RATE_MAX = 5;          // Tối đa N lệnh ...
constexpr uint32_t COMMAND_RATE_WINDOW_MS = 1000; // ... trong cửa sổ này

// -----------------------------------------------------------------------------
// IR
// -----------------------------------------------------------------------------
constexpr uint16_t IR_RX_BUFFER_SIZE = 1024;
constexpr uint8_t IR_RX_TIMEOUT_MS = 50;          // 50ms để thu được gói dài của điều hòa
constexpr uint16_t IR_RAW_MAX_LENGTH = 600;
constexpr uint16_t IR_DEFAULT_KHZ = 38;
constexpr uint32_t IR_LEARN_TIMEOUT_MS = 15UL * 1000;
constexpr uint8_t IR_MATCH_TOLERANCE_PERCENT = 25;
constexpr uint16_t IR_MATCH_MIN_TOLERANCE_US = 150;
constexpr uint8_t IR_LEARN_MAX_FAILS = 3;          // Quá số lần -> E_LEARN_FAIL
constexpr uint8_t LEARNED_CODE_QUEUE_SIZE = 4;     // Mã học từ nút chờ gửi khi mất mạng

// -----------------------------------------------------------------------------
// Giao diện: OLED, nút, LED
// -----------------------------------------------------------------------------
constexpr uint8_t OLED_I2C_ADDRESS = 0x3C;
constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;
constexpr uint32_t OLED_TIMEOUT_MS = 10UL * 1000;

// Nút: bấm ngắn = sáng OLED; giữ rồi thả ở mốc nào thì chạy chức năng mốc đó
constexpr uint16_t BUTTON_DEBOUNCE_MS = 30;
constexpr uint32_t BUTTON_LEARN_HOLD_MS = 5UL * 1000;   // Học lệnh IR
constexpr uint32_t BUTTON_SETUP_HOLD_MS = 10UL * 1000;  // Cài đặt: WiFi riêng + mã ghép đôi
constexpr uint32_t BUTTON_RESET_HOLD_MS = 20UL * 1000;  // Khôi phục gốc: xoá token + WiFi

// -----------------------------------------------------------------------------
// Âm thanh (DFPlayer)
// -----------------------------------------------------------------------------
constexpr uint32_t AUDIO_BAUD = 9600;
constexpr uint8_t AUDIO_VOLUME = 28;               // 0..30. PAM8403 khuếch đại ~24dB: để cao dễ rè
constexpr uint32_t AUDIO_BOOT_DELAY_MS = 1500;      // DFPlayer cần ~1.5s sau khi cấp điện để đọc thẻ
constexpr uint32_t AUDIO_MAX_PLAY_MS = 5UL * 1000;  // Không nhận được "phát xong" thì coi như xong

// -----------------------------------------------------------------------------
// Lỗi / log
// -----------------------------------------------------------------------------
constexpr uint8_t ERROR_REPEAT_THRESHOLD = 3;              // Lặp >= 3 lần ...
constexpr uint32_t ERROR_REPEAT_WINDOW_SEC = 24UL * 3600;  // ... trong 24h thì báo
constexpr uint32_t ERROR_MIN_RESEND_SEC = 3600;            // Cùng lỗi + thiết bị: 1 lần/giờ
constexpr size_t ERROR_LOG_CAPACITY = 50;                  // Hàng đợi gửi bù khi mất mạng

// -----------------------------------------------------------------------------
// Lưu trữ
// -----------------------------------------------------------------------------
constexpr const char* PROFILE_PATH = "/profile.json";
constexpr const char* PROFILE_BACKUP_PATH = "/profile.bak.json";
constexpr const char* PROFILE_TEMP_PATH = "/profile.tmp.json";

// -----------------------------------------------------------------------------
// Thời gian / ghép đôi
// -----------------------------------------------------------------------------
constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.google.com";
constexpr const char* TIMEZONE = "ICT-7";
constexpr uint32_t PAIRING_CODE_TTL_MS = 10UL * 60 * 1000;

// -----------------------------------------------------------------------------
// Vòng lặp chính
// -----------------------------------------------------------------------------
constexpr uint32_t LOOP_IDLE_MS = 10;              // Nhường CPU cho tác vụ nền / tiết kiệm điện
constexpr uint32_t DISPLAY_REFRESH_MS = 1000;

// Nhận request JSON qua Serial để thử khi chưa có app/Supabase (xem src/app/SerialBridge.h).
// Tắt khi phát hành: ai cắm USB vào mạch cũng gửi lệnh được.
constexpr bool SERIAL_BRIDGE_ENABLED = true;
constexpr size_t SERIAL_BRIDGE_MAX_LINE = 1024;
