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

## 5. Sóng WiFi khi đặt mạch trong hộp

Đo trên prototype (để trần, cách router một phòng): RSSI chỉ -67 đến -73 dBm, nhiều lần không vào được WiFi (`4WAY_HANDSHAKE_TIMEOUT`, `AUTH_EXPIRE`, `NO_AP_FOUND` trong log). Cho vào hộp sẽ còn yếu hơn. Anten PCB của module ESP32 rất nhạy với vật xung quanh:

- **Vùng cấm quanh anten:** đặt module ở **mép PCB**, phần anten chìa ra ngoài hoặc nằm trên vùng **không có đồng** (cả mặt trên lẫn mặt dưới, không đi dây, không đổ GND). Theo hướng dẫn thiết kế phần cứng của Espressif, tốt nhất anten cách mọi kim loại ≥ 15mm.
- **Giữ xa anten:** pin LiPo, nam châm loa, dây nguồn, mạch boost (cuộn cảm gây nhiễu), vỏ kim loại. Pin và loa nên đặt ở **đầu đối diện** với anten.
- **Vỏ hộp:** nhựa thường (ABS/PC). Không dùng kim loại, sơn ánh kim, mạ crom, hay nhựa pha bột carbon/sợi carbon (chặn sóng).
- Nếu vẫn yếu: dùng module có **cổng anten ngoài u.FL** (VD ESP32-WROOM-32U, ESP32-C3-MINI-1U) + anten dẻo dán vào thành hộp, phía hướng về router.
- **Nghiệm thu:** so RSSI của cùng một mạch đặt trần và đặt trong hộp, cùng vị trí (web bản thử và Presence đều báo RSSI). Đặt mức tụt tối đa cho phép, VD ≤ 6 dB.
- Hộp cần **cửa sổ trong suốt với hồng ngoại** (nhựa đen lọc IR hoặc nhựa trong) phía trước LED IR và mắt thu, nếu không thì phát và học lệnh đều yếu.
- Mạch chỉ dùng **WiFi 2.4GHz**. Trang cài WiFi và hướng dẫn sử dụng phải nói rõ điều này (nhiều router đặt tên mạng 2.4 và 5GHz khác nhau, hoặc chỉ phát tên phụ ở 5GHz).

## 6. Nhiệt khi không có tản nhiệt

Trong hộp kín không có tản nhiệt, nhiệt dồn lại, làm ESP32 nóng và **pin LiPo nóng theo**. Pin LiPo chỉ nên sạc ở khoảng 0–45°C, nên phải xử lý cả nguồn sinh nhiệt chứ không chỉ riêng ESP32:

| Nguồn nhiệt | Vì sao nóng | Cách xử lý |
|---|---|---|
| **AMS1117 trên DevKit** | Hạ 5V → 3.3V kiểu tuyến tính: ở 150–300mA (WiFi phát) đốt 0.25–0.5W thành nhiệt, thường **nóng hơn cả ESP32** | Bỏ DevKit; cấp 3V3 bằng LDO rớt áp thấp lấy thẳng từ pin (mục 3) hoặc buck: chênh áp nhỏ → gần như không sinh nhiệt |
| **ESP32 khi WiFi chạy liên tục** | 240MHz + radio bật suốt | Hạ CPU (80MHz khi chạy pin đã có trong `PowerManager`), bật Modem-Sleep / Light Sleep (giai đoạn 2 trong spec 5.2), không để radio phát liên tục |
| **TP4056 khi sạc** | Sạc tuyến tính: sạc 1A từ 5V vào pin 3.7V đốt ~1.3W, nóng nhất lúc đang sạc | Giảm dòng sạc (điện trở RPROG, VD ~500mA cho pin 1100mAh), hoặc IC sạc dạng xung (switching) như họ IP5306 ở mục 3 |
| **Mạch boost 5V** | Hiệu suất ~85–90%, phần hao thành nhiệt | Chỉ bật 5V khi phát IR / phát tiếng nếu được |

Thiết kế PCB và hộp:
- **Đổ đồng GND diện rộng + via nhiệt** dưới pad tản nhiệt của module ESP32 và dưới IC nguồn: tán nhiệt ra cả tấm PCB.
- Tách IC nguồn / sạc **xa pin**. Không dán pin sát lên ESP32 hay IC sạc.
- Khe thông gió trên và dưới hộp (khí nóng đi lên). Nếu cần kín, dùng **miếng dẫn nhiệt (thermal pad)** từ IC nguồn ra thành hộp.
- **Nghiệm thu:** chạy liên tục 24h trong hộp, có cả lúc đang sạc. Đo nhiệt độ bề mặt pin và ESP32 (nhiệt kế hồng ngoại hoặc cảm biến nội của chip). Đặt ngưỡng, VD pin < 45°C khi sạc.
- Firmware có thể đọc nhiệt độ: thêm cảm biến NTC sát pin; quá ngưỡng thì **ngừng sạc** (nếu IC sạc cho phép) và báo lỗi về server.

## 7. Việc khác nên làm trước khi phát hành

- Tắt nhận lệnh qua Serial: `SERIAL_BRIDGE_ENABLED = false` trong `config.h`.
- Dùng module MCU trần trên PCB riêng, bỏ chip USB-UART và đèn nguồn của DevKit (ăn điện thường trực).
- Âm thanh: cho phép tắt ampli khi im lặng (chân SHDN của PAM8403 hoặc ampli có shutdown). Có thể cân nhắc bỏ DFPlayer + thẻ nhớ, phát file nén từ flash qua I2S (VD MAX98357A) nếu flash đủ chỗ.
- Mỗi mạch có ID riêng in trên tem, token cấp qua ghép đôi (không nạp sẵn bí mật chung vào firmware).
