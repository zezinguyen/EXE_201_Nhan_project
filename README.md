# Hệ Thống Điều Khiển Thiết Bị Nhà Thông Minh Qua Hồng Ngoại & Supabase Cloud

Dự án điều khiển thiết bị gia dụng (Tivi LG/Samsung, Điều hòa Panasonic, Remote hồng ngoại tự học lệnh) thông qua giọng nói tiếng Việt 3 miền, tích hợp Zalo Mini App, Supabase Cloud Realtime và Mạch phần cứng ESP32.

---

## 📁 Cấu Trúc Thư Mục Dự Án (Project Structure)

```text
DoAn_SmartHome/
├── database/                        # Cơ sở dữ liệu Supabase PostgreSQL
│   └── supabase_schema.sql          # 12 bảng, 6 RPC Stored Procedures, RLS & Realtime
│
├── docs/                            # Tài liệu đặc tả kỹ thuật và giao thức
│   ├── specs.md                     # Đặc tả hệ thống v3 (Kiến trúc, phần cứng, luồng hoạt động)
│   ├── contracts.md                 # Hợp đồng giao thức kết nối WSS Phoenix, RPC, mã lỗi
│   └── production-notes.md          # Hướng dẫn sản xuất hàng loạt, tiết kiệm năng lượng, OTA
│
├── esp32/                           # Firmware C++ cho vi điều khiển ESP32 (PlatformIO)
│   ├── arduino/                     # Bản sketch Arduino IDE độc lập
│   │   └── SmartHome_ESP32_TV.ino
│   ├── data/                        # Dữ liệu LittleFS nạp vào Flash ESP32
│   │   └── profile.json             # Bộ đệm profile thiết bị cục bộ (Contract mục 5)
│   ├── include/                     # Toàn bộ file Header C++ (.h)
│   │   ├── config.h                 # Cấu hình chân GPIO, ngưỡng PIN, WiFi NVS
│   │   ├── AudioTracks.h            # Định nghĩa 9 track âm thanh MP3
│   │   ├── IrCode.h                 # Định dạng mã IR & hàm serialize/deserialize JSON
│   │   ├── AudioPlayer.h            # Điều khiển module DFPlayer Mini qua UART2
│   │   ├── ButtonHandler.h          # Xử lý nút bấm đa chức năng (Học lệnh, SoftAP, Reset)
│   │   ├── DeviceStore.h            # Quản lý LittleFS lưu profile và danh mục lệnh
│   │   ├── DisplayOled.h            # Màn hình OLED SSD1306 0.96 inch I2C
│   │   ├── IrLearner.h              # Mạch thu hồng ngoại VS1838B (Power-gated GPIO19)
│   │   ├── IrSender.h               # Phát xung IR (IRsend + IRac Panasonic)
│   │   ├── PowerManager.h           # Quản lý năng lượng, Modem-Sleep, đo ADC Pin/USB
│   │   ├── Provisioning.h           # SoftAP Captive Portal & mã PIN ghép đôi 6 số
│   │   ├── RealtimeClient.h         # Kết nối Supabase Realtime WSS (Phoenix Channels)
│   │   ├── StatusLed.h              # Đèn LED trạng thái RGB (Anode chung)
│   │   └── SupabaseRestClient.h     # Gửi HTTPS RPC (board_ack, board_status, offline queue)
│   ├── mp3/                         # 9 file âm thanh giọng nói tiếng Việt chuẩn cho DFPlayer
│   │   ├── 0001.mp3 ... 0009.mp3
│   │   └── README.md
│   ├── src/                         # Toàn bộ mã nguồn thực thi C++ (.cpp)
│   │   ├── main.cpp                 # Điểm khởi chạy chính & vòng lặp sự kiện ESP32
│   │   ├── AudioPlayer.cpp
│   │   ├── ButtonHandler.cpp
│   │   ├── DeviceStore.cpp
│   │   ├── DisplayOled.cpp
│   │   ├── IrLearner.cpp
│   │   ├── IrSender.cpp
│   │   ├── PowerManager.cpp
│   │   ├── Provisioning.cpp
│   │   ├── RealtimeClient.cpp
│   │   ├── StatusLed.cpp
│   │   └── SupabaseRestClient.cpp
│   ├── tools/                       # Công cụ hỗ trợ firmware
│   │   └── generate_mp3_tracks.js   # Script tải và sinh 9 file MP3 giọng đọc chuẩn
│   ├── min_spiffs.csv               # Bảng phân vùng Dual OTA (2 x 1.88MB App + LittleFS)
│   └── platformio.ini               # Cấu hình build PlatformIO, thư viện IRremoteESP8266...
│
├── zalo-app/                        # Ứng dụng điều khiển Zalo Mini App (React + Vite)
│   ├── src/
│   │   ├── assets/                  # Hình ảnh, biểu tượng giao diện
│   │   ├── services/                # Các tầng dịch vụ Cloud & IoT
│   │   │   ├── geminiService.js     # Tích hợp Google Gemini AI nhận diện giọng nói
│   │   │   ├── iotService.js        # Điều phối lệnh IoT, timeout 3s & Idempotency retry
│   │   │   ├── supabaseClient.js    # Khởi tạo Supabase client & kiểm tra kết nối
│   │   │   ├── supabaseSchema.js    # Schema SQL dự phòng và định nghĩa kiểu dữ liệu
│   │   │   └── supabaseService.js   # Đồng bộ Realtime Nhà, ghép nối PIN, gán phím học
│   │   ├── utils/                   # Các thư viện tiện ích xử lý logic
│   │   │   ├── audioCaptureManager.js      # Ghi âm Web Audio API, tính âm lượng, FFT
│   │   │   ├── audioCaptureManager.test.js # Unit test Vitest cho Audio Capture
│   │   │   ├── voiceCommandParser.js       # Phân tích ngữ nghĩa câu lệnh 3 miền, AC, YouTube
│   │   │   ├── voiceCommandParser.test.js  # Unit test Vitest cho Voice Parser (TDD)
│   │   │   └── wavUtils.js                 # Xử lý mã hóa Audio buffer sang định dạng WAV
│   │   ├── App.css                  # Giao diện Glassmorphism hiện đại
│   │   ├── App.jsx                  # Màn hình chính điều khiển thiết bị & kết nối
│   │   ├── index.css                # CSS cơ sở
│   │   └── main.jsx                 # Entry point React
│   ├── package.json
│   └── vite.config.js
│
├── spring-backend/                  # Backend REST proxy dự phòng (Spring Boot 3)
│   ├── pom.xml
│   └── src/
│
├── .gitignore                       # Cấu hình bỏ qua file nhị phân, build, node_modules
└── README.md                        # Tài liệu tổng quan dự án
```

---

## ⚡ Hướng Dẫn Chạy & Kiểm Thử

### 1. Zalo Mini App (`zalo-app`)
```bash
cd zalo-app

# Cài đặt thư viện phụ thuộc
npm install

# Chạy toàn bộ Unit Tests (TDD - Vitest)
npm test -- --run

# Chạy môi trường phát triển cục bộ
npm run dev

# Đóng gói bản phát hành sản phẩm
npm run build
```

### 2. Firmware ESP32 (`esp32`)
- Mở thư mục `esp32` bằng **VSCode** có cài extension **PlatformIO IDE**.
- Nạp firmware: Bấm **PlatformIO: Upload**.
- Nạp hệ thống tệp LittleFS (`profile.json`): Bấm **PlatformIO: Upload Filesystem Image**.
- Copy toàn bộ các file trong `esp32/mp3/` vào thư mục `mp3/` trên thẻ nhớ MicroSD cắm vào DFPlayer Mini.

### 3. Cơ Sở Dữ Liệu (`database`)
- Mở bảng điều khiển Supabase project của bạn.
- Truy cập vào **SQL Editor**, mở file [database/supabase_schema.sql](database/supabase_schema.sql) và bấm **Run**.
- Cấu hình biến môi trường `VITE_SUPABASE_URL` và `VITE_SUPABASE_ANON_KEY` trong ứng dụng Zalo Mini App.
