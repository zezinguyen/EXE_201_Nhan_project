# CLAUDE.md

Firmware ESP32 (chạy pin) cho SmartHome Voice Assistant: nhận lệnh từ Zalo Mini App **qua server** (Supabase: app gọi Edge Function → server gửi xuống kênh Realtime riêng của mạch → mạch trả kết quả bằng REST/RPC; app và mạch không nói chuyện trực tiếp), phát IR điều khiển Tivi/Điều hòa, gán phím remote ảo. Thiết kế phải chạy đúng khi **một nhà nhiều mạch** và **một mạch nhiều người dùng** (spec mục 4.4). Spec đầy đủ: [../docs/specs.md](../docs/specs.md) — đọc mục liên quan trước khi sửa một module.
Đây là bản **prototype**; các thay đổi dự kiến khi làm đại trà (đổi MCU, OTA, module nguồn, WS2812) ghi ở [docs/production-notes.md](../docs/production-notes.md) — chưa làm, đừng tự triển khai khi chưa được yêu cầu.
Giao tiếp với backend/app (Realtime, REST, profile, mã IR) chốt trong [docs/contracts.md](../docs/contracts.md): **đổi payload nào trong firmware thì sửa contract cùng lúc**.

## Lệnh thường dùng

Mọi lệnh `pio` chạy trong thư mục `firmware/` (gốc project PlatformIO); tài liệu chung ở `../docs/`.

```bash
pio run -e esp32dev                  # build
pio run -e esp32dev -t upload        # nạp firmware
pio run -e esp32dev -t uploadfs      # nạp data/ (profile.json mẫu) vào LittleFS
pio device monitor -e esp32dev       # serial monitor (có giải mã exception)
pio test -e native                   # unit test trên máy tính (lib/core)
pio test -e native -f test_backoff   # chạy một bộ test
pio test -e esp32dev -f test_device_feedback   # test trên mạch thật: request giả -> IR + âm thanh
pio test -e esp32dev -f test_device_hardware -v # kiểm tra nối dây từng linh kiện, OLED hướng dẫn, nút để chuyển bước
pio run -e esp32dev_local -t upload  # BẢN THỬ không cần server (xem mục dưới)
```

**Bản thử không cần server** (`env:esp32dev_local`, code ở `src/local/`, thay cho `main.cpp`): chưa có WiFi thì bật WiFi riêng (tên + mật khẩu trên OLED) → laptop nối vào, trang cài WiFi tự mở → vào WiFi nhà thì OLED hiện IP, mở `http://smarthome.local` để điều khiển điều hòa (mẫu Funiki R32), thử/dò giao thức, học và bấm phím. Phím học và giao thức lưu thẳng vào profile trên LittleFS (`ProfileEdit`). Web không có đăng nhập → **chỉ để thử, không đưa vào bản phát hành** (spec v3 đã bỏ điều khiển cục bộ).

Thử mạch không cần app: mở `pio device monitor`, gõ một dòng JSON (bật `SERIAL_BRIDGE_ENABLED` trong config.h):
`{"event":"command","payload":{"commandId":"c1","deviceId":"tv_living","action":"POWER","seq":1}}` → xem log `ACK ...` và nghe âm thanh.

Lần đầu: `cp include/secrets.example.h include/secrets.h` rồi điền giá trị thật (file này đã `.gitignore`).

## Quy tắc bắt buộc

1. **PlatformIO** là hệ build duy nhất. Không thêm Makefile/CMake/Arduino IDE sketch.
2. **`platformio.ini` tách theo khối**:
   - `[common]` cờ build chung; `[arduino_libs]` thư viện chạy trên mọi core Arduino.
   - `[<mcu>_base]` (VD `[esp32_base]`): `platform`, `lib_deps` riêng của họ MCU, `build_src_filter` trỏ vào `src/platform/<mcu>/`.
   - `[env:<board>]`: chỉ `extends`, `board`, và `-DBOARD_xxx`. Ngoại lệ: env bản thử (`esp32dev_local`) thêm `build_src_filter` lấy từ `[local_web]`.
   - Thư viện mới: chạy được mọi MCU → `[arduino_libs]`; chỉ một họ MCU → `[<mcu>_base]`. Luôn ghim phiên bản (`@^x.y.z`).
3. **`include/config.h` là nơi duy nhất** đặt chân GPIO và thông số (thời gian, ngưỡng, kích thước bộ đệm, đường dẫn file). Module không viết số "magic". Chân đặt trong khối `#if defined(BOARD_xxx)`. Bí mật (URL, key, CA) ở `include/secrets.h`.
4. **Mỗi module có `.h` + `.cpp`**:
   - Code dùng chung được cho mọi MCU → `src/modules/Foo.h` + `src/modules/Foo.cpp`.
   - Code phụ thuộc MCU → header vẫn ở `src/modules/Foo.h`, triển khai ở `src/platform/<mcu>/Foo.cpp` (include bằng `"modules/Foo.h"`). Một lớp có thể tách đôi: phần chung ở `modules/`, phần riêng ở `platform/` (VD `DeviceStore`).
   - Không dùng `#ifdef ESP32` rải rác trong module chung — tách sang `platform/` thay vì vậy.
5. **Đặt tên**: hàm và biến **camelCase** (`sendCode`, `batteryPercent`) — **không dùng snake_case**. Lớp/enum/struct PascalCase. Thành viên private có hậu tố `_`. Hằng trong `config.h` UPPER_SNAKE_CASE; hằng cục bộ `kName`. (Khoá JSON giữ đúng như spec/Supabase, VD `irac_protocol`.)
6. **Viết test khi test được**: logic không đụng phần cứng đặt trong `lib/core` (C++ thuần, không include `Arduino.h`) và có test Unity trong `test/test_<tên>/test_main.cpp`. Test cần phần cứng thật đặt tên `test/test_device_<tên>/` (env native tự bỏ qua; được dùng code trong `src/` vì `test_build_src = yes`, `main.cpp` tắt khi `PIO_UNIT_TESTING`). Thời gian, số ngẫu nhiên... truyền vào làm tham số để test được (xem `Backoff::nextDelayMs(randomValue)`). Chạy `pio test -e native` trước khi báo xong.
7. Không chặn vòng lặp: không `delay()` dài trong module, dùng `update(nowMs)` + máy trạng thái (xem `AudioPlayer`). `millis()` tràn số → luôn so sánh dạng `nowMs - startMs >= duration`.

## Cấu trúc

```
include/config.h          chân + thông số (theo board)
include/secrets*.h        URL/key Supabase, CA
lib/core/src/             logic thuần C++, có unit test:
  Backoff                 khoảng chờ kết nối lại tăng dần + jitter
  CommandGuard            hạn dùng, chống trùng commandId, seq khi chưa có NTP, giới hạn tốc độ
  RateLimiter, RingBuffer, IrCompare, BatteryGauge, ButtonDetector, ErrorThrottle, ErrorCodes,
  Password                mật khẩu ngẫu nhiên cho WiFi cài đặt (hiện trên OLED)
  ProfileEdit             sửa profile trên mạch (bản thử): thêm/xoá phím học, đổi giao thức điều hòa
src/main.cpp              chỉ khởi tạo + nối module
src/app/CommandHandler    broadcast từ app -> kiểm tra -> phát IR -> ACK; học lệnh (từ app hoặc từ nút)
src/app/Feedback          sự kiện -> file âm thanh (mp3)
src/app/SerialBridge      nhận request JSON qua Serial để thử khi chưa có app
src/app/Outbox            hàng đợi ghi Supabase qua REST: mã học từ nút (learned_codes), báo lỗi (error_reports)
src/modules/              header của mọi module + phần triển khai dùng chung
src/platform/esp32/       triển khai riêng ESP32: Board, PowerManager, WifiManager, SetupPortal (trang cài WiFi),
                          SecureStore (NVS), TimeSync, AudioPlayer (UART), DeviceStore (LittleFS),
                          SupabaseRest (HTTPS)
src/local/                BẢN THỬ (env esp32dev_local): LocalMain thay main.cpp, LocalWebServer (web + mDNS)
data/profile.json         profile mẫu (định dạng bản sao trên mạch)
sounds/                   9 file mp3 chép vào thẻ nhớ DFPlayer (thư mục /mp3/)
test/test_*/              mỗi thư mục một bộ test Unity
```

Ánh xạ với spec mục 11: `power_manager`→`PowerManager`, `wifi_manager`→`WifiManager`, `realtime_client`→`RealtimeClient`, `provisioning`→`Provisioning`+`SecureStore`, `ir_sender`→`IrSender`, `ir_learner`→`IrLearner`, `device_store`→`DeviceStore`, `error_reporter`→`ErrorReporter`, `buttons`→`UserButton` (1 nút: ngắn = OLED, giữ 5s học lệnh, 10s cài đặt, 20s reset), `display_oled`→`DisplayOled`, `audio_player`→`AudioPlayer`.

## Thêm MCU / board mới

1. `platformio.ini`: thêm `[<mcu>_base]` + `[env:<board>]` với `-DBOARD_<TÊN>`.
2. `config.h`: thêm khối `#elif defined(BOARD_<TÊN>)` khai báo đủ mọi `PIN_*`.
3. Tạo `src/platform/<mcu>/` và triển khai mọi header có phần riêng MCU (danh sách ở trên). Thiếu file nào linker sẽ báo.
4. Board khác cùng họ MCU chỉ cần bước 1 (dùng lại `[<mcu>_base]`) và 2.

## Lưu ý kỹ thuật (dễ sai)

- **Bảo mật**: không bao giờ đưa `service_role` key vào firmware; không `setInsecure()` — luôn kiểm tra CA. Không log mật khẩu WiFi, token, SSID.
- **Pin**: đo pin chỉ dùng chân ADC1 (GPIO32–39) vì ADC2 chết khi bật WiFi. GPIO34/35 không có pull-up trong. Không dùng Deep Sleep (đứt WebSocket). Light Sleep tự động (giai đoạn 2) cần `CONFIG_PM_ENABLE`, Arduino-ESP32 dựng sẵn có thể không bật.
- **Mắt thu IR**: prototype nối VCC thẳng 3V3 (`PIN_IR_RX_POWER = -1`), bộ thu chỉ bật khi học lệnh; đặt chân GPIO vào `PIN_IR_RX_POWER` thì firmware tự cấp/ngắt nguồn mắt thu. LED RGB là loại cathode chung. DFPlayer (3V3) + PAM8403 (5V) cấp điện thường trực để phát tiếng ngay; DFPlayer cần ~1.5s sau khi bật mới nhận lệnh (`AudioPlayer` tự giữ lệnh phát chờ).
- **Âm thanh**: mọi chỗ phát tiếng đi qua `app/Feedback`; số file khớp `AudioTrack` (`lib/core/src/AudioTracks.h`), thẻ nhớ đặt `/mp3/0001.mp3`... (bảng ở spec mục 3.1).
- **Realtime**: heartbeat Phoenix (topic `phoenix`) ~25s là bắt buộc; quá 10s không trả lời thì kết nối lại. Đích: kênh private `realtime:board:<boardId>`, mạch chỉ nghe, chỉ server gửi; ACK/học lệnh/trạng thái gửi lên bằng RPC. Firmware hiện còn dùng kênh `home:<homeId>` + broadcast + Presence (đánh dấu 🔁 trong contracts.md), cần đổi.
- **Lệnh**: quá hạn/seq cũ → bỏ qua không ACK; trùng `commandId` → ACK lại nhưng không phát IR. Đang gán phím thì không phát IR (`E_BUSY`).
- **Mã IR 64 bit** gửi/nhận dạng chuỗi hex (`"0xE0E040BF"`) vì JS không giữ đủ chính xác. Mã điều hòa (giao thức có state > 64 bit: GREE, MIDEA…) khi học được lưu dạng `raw`; tên giao thức nhận ra nằm ở `IrCapture::detected`.
- **Cổng 80**: trang cài WiFi (`SetupPortal`) và web bản thử dùng chung cổng → web bản thử chỉ bật khi WiFi cài đặt đã tắt.
- Flash: IRac chiếm nhiều chỗ → dùng bảng phân vùng `min_spiffs.csv` (app ~1.9MB, LittleFS 128KB).

## Trạng thái hiện tại

Khung đã build được và test `lib/core` đã chạy qua; chưa chạy thử trên phần cứng. Trang cài WiFi trên AP đã có (`SetupPortal`). Các phần còn `TODO(tuần N)` theo lộ trình spec mục 13: đăng ký ghép đôi với Edge Function, tải profile qua HTTPS khi `profileVersion` đổi, lưu hàng đợi Outbox (lỗi + mã học) vào LittleFS, tạo bảng `learned_codes`/`error_reports` + RLS phía Supabase, làm mới token Realtime, lưu trạng thái điều hòa vào NVS, tiết kiệm pin giai đoạn 2. Chưa chuyển sang giao tiếp qua server: đổi kênh sang `board:<boardId>`, gửi ACK/sự kiện học lệnh/trạng thái bằng RPC thay cho broadcast + Presence (contracts.md, các mục 🔁).
