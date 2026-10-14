#pragma once
#include <Arduino.h>

// =========================================================================
// SƠ ĐỒ CHÂN PHẦN CỨNG ESP32 DEVKIT 30 CHÂN (Specs mục 3.3)
// =========================================================================

// 1. Ngoại vi Hồng ngoại (IR)
#define PIN_IR_SEND        4   // Chân phát IR qua trở 330Ω vào cực B transistor S8050
#define PIN_IR_RECV_OUT   18   // Chân tín hiệu OUT của mắt thu VS1838B
#define PIN_IR_RECV_VCC   19   // Cấp nguồn VCC cho mắt thu qua GPIO (chỉ bật khi học lệnh)

// 2. DFPlayer Mini (UART2)
#define PIN_DFPLAYER_RX   16   // ESP32 RX2 nối vào TX của DFPlayer Mini
#define PIN_DFPLAYER_TX   17   // ESP32 TX2 nối vào RX của DFPlayer Mini

// 3. Màn hình OLED SSD1306 0.96" I2C
#define PIN_OLED_SDA      21   // D21 SDA
#define PIN_OLED_SCL      22   // D22 SCL
#define OLED_I2C_ADDR   0x3C   // Địa chỉ I2C màn hình OLED

// 4. LED RGB (Anode chung: Anode nối 3V3, GPIO xuất mức LOW để sáng)
#define PIN_RGB_R         25   // Đỏ qua trở 220Ω
#define PIN_RGB_G         26   // Xanh lá qua trở ~47Ω
#define PIN_RGB_B         27   // Xanh dương qua trở ~47Ω

// 5. Nút nhấn đa năng (1 nút duy nhất, Spec 0.5)
#define PIN_BUTTON        13   // Nút nhấn nối GND (sử dụng INPUT_PULLUP)

// 6. Đo điện áp Pin và sạc USB (ADC1)
#define PIN_BATTERY_ADC   36   // Chân VP (GPIO36), qua cầu chia 100k - 100k đo B+
#define PIN_USB_ADC       39   // Chân VN (GPIO39), qua cầu chia 100k - 100k đo IN+ sạc

// =========================================================================
// CẤU HÌNH THỜI GIAN VÀ TIẾT KIỆM PIN (Specs mục 5)
// =========================================================================
#define CPU_FREQ_MHZ                 80      // Hạ xung CPU xuống 80MHz tiết kiệm điện
#define OLED_SLEEP_TIMEOUT_MS        10000   // Tự tắt OLED sau 10 giây không có thao tác
#define STATUS_REPORT_INTERVAL_MS    300000  // Gửi trạng thái pin/RSSI lên server mỗi 5 phút
#define REALTIME_HEARTBEAT_MS        25000   // Heartbeat Supabase Phoenix 25 giây
#define REALTIME_HEARTBEAT_TIMEOUT   10000   // Quá 10s không có phản hồi coi như đứt kết nối
#define LEARN_TIMEOUT_MS             15000   // Chờ bấm remote học lệnh tối đa 15 giây
#define PAIRING_MODE_TIMEOUT_MS      600000  // Chế độ ghép đôi tồn tại 10 phút

// =========================================================================
// THƯ MỤC LƯU TRỮ VÀ GIỚI HẠN
// =========================================================================
#define PROFILE_FILE_PATH            "/profile.json"
#define MAX_DEDUP_CACHE_SIZE         16      // Lưu 16 commandId gần nhất chống lệnh lặp
#define MAX_COMMANDS_PER_SECOND      5       // Giới hạn tốc độ phát IR: tối đa 5 lệnh/giây
#define MAX_OFFLINE_LEARNED_CODES    4       // Tối đa 4 mã học offline lưu tạm khi mất mạng
#define MAX_OFFLINE_ERROR_REPORTS    50      // Hàng đợi lưu tối đa 50 log lỗi vòng lặp

// =========================================================================
// TÙY CHỌN DÀNH CHO BẢN PHÁT HÀNH
// =========================================================================
#define FIRMWARE_VERSION             "0.1.0"
#define SERIAL_BRIDGE_ENABLED        true    // Nhận JSON qua Serial để test nhanh (Contract mục 8)

