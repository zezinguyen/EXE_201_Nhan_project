# Việc còn dở và phần làm tạm

Cập nhật 16/10/2026, sau khi thử app ↔ Supabase ↔ mạch trên phần cứng thật (điều hòa Funiki R32, Tivi TCL).

## 1. App chưa có nút cho nhiều phím Tivi

Mạch **đã lưu mã** đủ 26 phím Tivi TCL (học từ remote thật, hoặc chọn hãng TCL trong "Chọn hãng / học phím"), nhưng thẻ Tivi trên app mới có nút **Nguồn, Tăng/Giảm âm lượng, Tắt tiếng, YouTube**. Các phím sau có mã trên mạch nhưng **chưa có nút trên app** và chưa có lệnh giọng nói:

| Nhóm       | Phím (`action`)                                      |
| ---------- | ---------------------------------------------------- |
| Phím số    | `NUM_0` … `NUM_9`                                    |
| Điều hướng | `UP`, `DOWN`, `LEFT`, `RIGHT`, `OK`, `BACK`          |
| Menu       | `HOME`, `MENU`, `LIST`, `INPUT`                      |
| Kênh       | `CH_UP`, `CH_DOWN`                                   |

Việc cần làm: remote ảo đầy đủ trên thẻ Tivi (bàn phím số, mũi tên + OK, Menu/Home/Back/List/Input, chuyển kênh), dùng danh sách ở `zalo-app/src/services/tvKeys.js`; thêm lệnh giọng nói "chuyển kênh 5", "kênh tiếp"... trong `voiceCommandParser.js`.

## 2. Phần làm tạm (phải thay trước khi phát hành)

| Chỗ | Đang làm | Cần đổi thành |
| --- | --- | --- |
| `firmware/src/modules/RealtimeClient.cpp` | Vào kênh `home:<homeId>` **public** (`private = false`): Supabase chưa có RLS cho kênh private, anon key bị `Unauthorized` | Kênh private `board:<boardId>` + RLS + `boardToken` riêng từng mạch (contracts.md mục 2, 3.1) |
| App gửi lệnh | Gửi thẳng `command` vào kênh `home:<homeId>`; RPC `send_command` lỗi thì **tự gửi thẳng** (che mất lỗi DB) | Qua Edge Function (contracts.md mục 9); RPC lỗi thì báo lỗi |
| `firmware/src/app/ProfileSetup` + `zalo-app/src/components/AcSetupModal.jsx`, `TvSetupModal.jsx` | App sửa profile trên mạch qua Realtime (contracts.md mục 3.8) | Server quản lý profile, gửi `profile_updated` (mục 3.7) |
| `zalo-app` (`App.jsx`, `iotService.js`, `voiceCommandParser.js`) | Ghi cứng `ac_funiki`, `tv_living` (khớp profile trên mạch thử) | Lấy danh sách thiết bị từ DB / profile của mạch |
| Bảng `home_devices` | Chỉ có `tv_living`, `ac_living`; thiếu `ac_funiki` → RPC trả `E_DEVICE_NOT_FOUND` | Thống nhất id thiết bị giữa DB, app và profile trên mạch |
| `firmware/src/main.cpp` | Nạp sẵn WiFi + ghép đôi từ `DEFAULT_*` trong `secrets.h`, thiếu `DEFAULT_BOARD_TOKEN` thì dùng **anon key** làm token | Ghép đôi thật (mục 3) |
| `zalo-app/src/services/learnedCodeLog.js` | Mã học được lưu `localStorage` của trình duyệt để xuất JSON | Lưu ở server cùng profile |

## 3. Ghép đôi chưa chạy đầu–cuối

Mạch hiện mã 6 số trên OLED nhưng **chưa gửi mã lên server** và chưa hỏi lại để nhận `homeId` + token, nên RPC `pair_board` của app không tìm thấy mạch. Cần: firmware gọi `board-register` (hoặc RPC tạm) sau khi vào WiFi rồi hỏi lại tới khi nhận token; Supabase lưu hash mã + hạn 10 phút (contracts.md mục 7).

## 4. Khác

- **Gemini API key lộ**: `VITE_GEMINI_API_KEY` bị nhúng vào JS gửi cho người dùng → gọi Gemini qua server.
- **Build app**: `npm ci` cần `--legacy-peer-deps` (`zmp-vite-plugin` chỉ nhận vite 4/5, dự án dùng vite 8); bundle ~570 kB (nên tách nhỏ); còn ~25 cảnh báo oxlint trong `App.jsx` (biến/import không dùng).
- **Tivi**: IR một chiều nên app chỉ đoán trạng thái bật/tắt; nói "bật tivi" khi Tivi đang bật sẽ tắt nó (cùng mã `POWER`).
- `spring-backend` chưa có test.
