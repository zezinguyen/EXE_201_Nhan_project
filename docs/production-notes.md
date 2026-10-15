# Ghi chú khi làm đại trà

> Bản hiện tại là **prototype** (ESP32 DevKit + module rời). Các mục dưới đây chưa làm, chỉ ghi lại để áp dụng khi chuyển sang sản xuất số lượng. Số liệu và tên linh kiện cần kiểm tra lại datasheet / giá tại thời điểm làm.

## 1. Đổi sang MCU rẻ hơn ESP32

ESP32 (2 nhân, nhiều ngoại vi) thừa so với nhu cầu. Firmware thực sự cần:

- WiFi + **2 kết nối TLS cùng lúc** (WSS Realtime + HTTPS REST) → cần RAM đủ cho mbedTLS
- Phát/thu IR (RMT), 1 UART (DFPlayer), I2C (OLED), 2 ADC (pin, USB), ~6 GPIO
- Flash: firmware hiện ~1.24MB (phần lớn là IRac) → cần **≥ 4MB** nếu muốn OTA 2 phân vùng

| Lựa chọn | Nhận xét |
|---|---|
| **ESP32-C3** (module ESP32-C3-MINI / ESP-C3-12F) | **Ứng viên chính.** Rẻ hơn ESP32, cùng core Arduino-ESP32 nên phần lớn code `firmware/src/platform/esp32/` dùng lại được, có RMT cho IR, đủ RAM cho 2 TLS. Lưu ý: chỉ ADC1 (GPIO0–4) dùng được khi bật WiFi; ít GPIO hơn (~15) nhưng vẫn đủ. Cần kiểm tra IRremoteESP8266 chạy ổn trên C3 |
| ESP32-C2 / ESP8684 | Rẻ nhất họ ESP, nhưng RAM ít hơn và nhiều bản chỉ 2MB flash → không đủ chỗ cho OTA với IRac. Chỉ cân nhắc nếu cắt bớt IRac |
| ESP8266 | Không nên: RAM quá ít cho 2 kết nối TLS cùng lúc |

Cách chuyển trong dự án (xem firmware/CLAUDE.md "Thêm MCU / board mới"): thêm `[esp32c3_base]` + `[env:<board>]` trong `firmware/platformio.ini`, khối `BOARD_xxx` trong `config.h`, rồi kiểm tra từng file trong `firmware/src/platform/esp32/` (có thể dùng chung, chỉ tách file nào khác).

## 2. Cập nhật firmware qua OTA

Nạp lại bằng cáp cho từng nhà là không khả thi. Luồng đề xuất:

```
1. Upload firmware.bin lên Supabase Storage, ghi version + SHA-256 vào bảng
2. Admin chọn mạch/đợt → server gửi broadcast "ota" {version, url, sha256} vào kênh board:<boardId> của từng mạch
3. Mạch kiểm tra: đã ghép đôi, version mới hơn, pin > 30% hoặc đang cắm sạc
4. Đóng Realtime (giải phóng RAM) → tải qua HTTPS có kiểm tra CA → ghi vào phân vùng trống
   → so SHA-256 → khởi động lại
5. Bản mới vào được Supabase → đánh dấu "chạy tốt" (esp_ota_mark_app_valid_cancel_rollback)
   Không vào được → lần khởi động sau tự quay về bản cũ (rollback)
```

- Phân vùng hiện tại (`min_spiffs.csv`) đã có 2 vùng app 1.88MB; Arduino-ESP32 bản dựng sẵn đã bật `CONFIG_APP_ROLLBACK_ENABLE`.
- Khi đại trà nên có thêm: **ký số firmware**, **Secure Boot**, **Flash Encryption** (spec 10.1, "để đợt sau"). Các tính năng này khó hoàn tác, chỉ bật khi firmware đã ổn định.
- Cập nhật theo đợt (một nhóm nhỏ trước), không đẩy cho mọi nhà cùng lúc.

## 3. Module sạc tích hợp boost

Bản prototype dùng TP4056 + module boost rời + AMS1117 trên DevKit: nhiều dây, nhiều điểm hỏng, và hao điện (boost → ổn áp tuyến tính, xem spec 3.2, 5.4).

Thay bằng IC/module "power bank" gộp sạc + boost 5V + bảo vệ pin (VD họ **IP5306**, IP5209/IP5310…). Cần lưu ý:

- **Tự tắt khi tải nhẹ**: nhiều IC power bank tự ngắt 5V nếu dòng tải dưới vài chục mA trong một khoảng thời gian. Mạch ở chế độ tiết kiệm pin có thể rơi vào ngưỡng này → mạch tắt. Chọn bản có I2C cho phép tắt tính năng này (VD IP5306-I2C), hoặc thiết kế để không phụ thuộc vào nó.
- Nên có chân/I2C báo **đang sạc / đầy / mức pin** để bỏ cầu chia đo USB và đoán trạng thái sạc bằng điện áp (bản hiện tại không có chân CHRG).
- Cân nhắc cấp **3V3 cho MCU bằng LDO rớt áp thấp lấy thẳng từ pin** (ME6211, HT7333…) thay vì boost 5V rồi hạ xuống; chỉ dùng 5V cho LED IR và ampli. Đây là thay đổi ảnh hưởng thời lượng pin nhiều nhất.

## 4. Thay LED RGB bằng WS2812 (LED địa chỉ)

- Chỉ tốn **1 GPIO** (thay vì 3), đủ màu, chỉnh độ sáng bằng phần mềm, bỏ được 3 điện trở.
- Firmware: chỉ cần viết lại `firmware/src/modules/StatusLed.cpp` (header giữ nguyên), dùng RMT để xuất tín hiệu.
- Lưu ý:
  - WS2812B cần nguồn ~5V, nhưng mức logic "1" của nó là 0.7 × VDD ≈ 3.5V, **cao hơn 3.3V** của MCU → cần IC chuyển mức (VD 74AHCT1G125) hoặc chọn loại chạy được ở 3.3V (SK6812, WS2812C-2020…).
  - Mỗi LED ăn **~0.5–1mA ngay cả khi tắt**. Với thiết bị chạy pin nên cấp nguồn LED qua một chân/transistor để ngắt hẳn khi không cần.

## 5. Việc khác nên làm trước khi phát hành

- Tắt nhận lệnh qua Serial: `SERIAL_BRIDGE_ENABLED = false` trong `config.h`.
- Dùng module MCU trần trên PCB riêng, bỏ chip USB-UART và đèn nguồn của DevKit (ăn điện thường trực).
- Âm thanh: cho phép tắt ampli khi im lặng (chân SHDN của PAM8403 hoặc ampli có shutdown). Có thể cân nhắc bỏ DFPlayer + thẻ nhớ, phát file nén từ flash qua I2S (VD MAX98357A) nếu flash đủ chỗ.
- Mỗi mạch có ID riêng in trên tem, token cấp qua ghép đôi (không nạp sẵn bí mật chung vào firmware).
