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
├── firmware/                        # Firmware mạch ESP32 (PlatformIO) – quy tắc ở firmware/CLAUDE.md
│   ├── include/                     # config.h (chân GPIO, thông số theo board), secrets.example.h
│   ├── lib/core/                    # Logic C++ thuần, có unit test (Backoff, CommandGuard, BatteryGauge...)
│   ├── src/
│   │   ├── main.cpp                 # Chỉ khởi tạo + nối module
│   │   ├── app/                     # CommandHandler, Feedback, Outbox, SerialBridge
│   │   ├── modules/                 # Header mọi module + phần dùng chung (IrSender, RealtimeClient...)
│   │   └── platform/esp32/          # Phần riêng ESP32 (WiFi, NVS, LittleFS, HTTPS, DFPlayer...)
│   ├── test/                        # Unit test Unity (native) + test_device_* chạy trên mạch thật
│   ├── data/profile.json            # Profile mẫu nạp vào LittleFS
│   ├── sounds/                      # 9 file 0001–0009.mp3 cho thẻ nhớ DFPlayer
│   └── platformio.ini
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

### 2. Firmware ESP32 (`firmware`)
```bash
cd firmware
cp include/secrets.example.h include/secrets.h   # điền URL/key Supabase thật (đã .gitignore)

pio test -e native               # unit test trên máy tính
pio run -e esp32dev -t upload    # build + nạp firmware
pio run -e esp32dev -t uploadfs  # nạp data/profile.json vào LittleFS
pio device monitor -e esp32dev   # serial monitor
```
- Mở thư mục `firmware/` bằng VSCode có extension **PlatformIO IDE**.
- Chép `firmware/sounds/0001.mp3 ... 0009.mp3` vào thư mục `mp3/` trên thẻ microSD của DFPlayer Mini.
- Chi tiết quy tắc code, sơ đồ module: [firmware/CLAUDE.md](firmware/CLAUDE.md).

### 3. Cơ Sở Dữ Liệu (`database`)
- Mở bảng điều khiển Supabase project của bạn.
- Truy cập vào **SQL Editor**, mở file [database/supabase_schema.sql](database/supabase_schema.sql) và bấm **Run**.
- Cấu hình biến môi trường `VITE_SUPABASE_URL` và `VITE_SUPABASE_ANON_KEY` trong ứng dụng Zalo Mini App.
