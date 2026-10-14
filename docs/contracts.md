# Contract: Backend / App ↔ Mạch IR (firmware)

Tài liệu này mô tả **chính xác những gì mạch gửi và nhận**, để phía Supabase (bảng, RLS, Edge Functions) và Zalo Mini App làm khớp. Mọi thông tin đều lấy từ firmware hiện tại (`v0.1.0`).

- ✅ = firmware **đã làm**, gửi/nhận đúng như mô tả.
- 🚧 = firmware **chưa làm**, đây là đề xuất. Cần thống nhất trước khi một bên bắt đầu code.
- 🔁 = firmware **đang làm theo cách cũ** (app ⇄ mạch qua kênh chung của nhà), **phải đổi** sang cách mô tả ở đây. Backend/app làm theo mô tả này, không làm theo cách cũ.

> **Nguyên tắc đã chốt: app và mạch chỉ giao tiếp qua server.** App không gửi gì thẳng cho mạch và không nghe kênh của mạch, kể cả qua Realtime. Mọi lệnh đi App → Edge Function → kênh riêng của mạch; mọi kết quả đi Mạch → REST/RPC → DB → App. Lý do và yêu cầu "nhiều mạch / nhiều người dùng" xem [specs.md](specs.md) mục 4 và 4.4.

Bối cảnh chung (vai trò, luồng nghiệp vụ) xem [specs.md](specs.md). Khi đổi contract thì sửa file này **cùng lúc** với firmware.

---

## 1. Tổng quan kênh giao tiếp

```
 Zalo Mini App ──HTTPS: send-command, learn-start…──►┐
               ◄──Realtime home:<homeId> (chỉ đọc)───┤   SERVER (Supabase)
                                                     │   Edge Functions + DB
                     ┌──broadcast "command",─────────┤
                     │  "learn_start", "profile_updated"
                     ▼   kênh board:<boardId>        │
                 Mạch IR ──REST/RPC: ack, learn event, status,──►┘
                 (WSS, token riêng)   learned_codes, error_reports
```

| Kênh                                     | Hướng          | Dùng cho                                                                    |
| ---------------------------------------- | -------------- | --------------------------------------------------------------------------- |
| Edge Functions (HTTPS)                   | App → Server   | Ra lệnh, học lệnh, ghép đôi (mục 9)                                         |
| Realtime **Broadcast** `board:<boardId>` | Server → Mạch  | Lệnh, bắt đầu/huỷ học lệnh, báo profile mới. **Chỉ server gửi**            |
| **REST/RPC** (PostgREST)                 | Mạch → Server  | ACK, tiến trình học lệnh, trạng thái pin/RSSI, mã học từ nút, báo lỗi       |
| Realtime `home:<homeId>` / bảng          | Server → App   | Kết quả lệnh, trạng thái thiết bị, mạch online/offline. **Chỉ server gửi** |

Mạch **không mở cổng nào**: mọi kết nối đều do mạch chủ động mở ra Supabase.

Firmware hiện tại 🔁: mạch vào kênh `home:<homeId>`, nhận lệnh broadcast thẳng từ app, trả ACK/học lệnh bằng broadcast, báo pin bằng Presence. Phần REST (`learned_codes`, `error_reports`) đã đúng hướng.

---

## 2. Xác thực mạch

| Mục                         | Giá trị                                                     | Trạng thái |
| --------------------------- | ----------------------------------------------------------- | ---------- |
| Header REST `apikey`        | Anon key của project (nạp sẵn trong firmware)               | ✅         |
| Header REST `Authorization` | `Bearer <boardToken>`                                       | ✅         |
| Realtime `access_token`     | `<boardToken>`, gửi trong payload `phx_join`                | ✅         |
| Cấp `boardToken`            | Edge Function cấp sau khi ghép đôi bằng mã 6 số (spec 10.2) | 🚧         |
| Làm mới token               | Event `access_token` của Realtime                           | 🚧         |

Mạch lưu 3 giá trị nhận được khi ghép đôi: `boardToken`, `boardId`, `homeId`. Yêu cầu với `boardToken` (🚧, đề xuất):

- Là JWT mà Supabase chấp nhận, có `role = "authenticated"`, để PostgREST và Realtime private channel nhận.
- Có claim nhận diện mạch, ví dụ `board_id`, `home_id`, để RLS kiểm tra: mạch chỉ ghi vào bảng với đúng `home_id`/`board_id` của mình, chỉ **nghe** được kênh `board:<board_id>` của mình và **không gửi** được vào kênh nào (tiêu chí nghiệm thu 11, 12, 19).
- Firmware giữ token tối đa **511 ký tự**.
- **Không bao giờ** dùng `service_role` cho mạch.

---

## 3. Realtime

### 3.1 Kết nối (kênh 🔁, heartbeat/kết nối lại ✅)

| Mục          | Giá trị                                                                                                               |
| ------------ | --------------------------------------------------------------------------------------------------------------------- |
| URL          | `wss://<project>.supabase.co/realtime/v1/websocket?apikey=<anon>&vsn=1.0.0`                                           |
| Topic        | `realtime:board:<boardId>` (firmware hiện tại 🔁 `realtime:home:<homeId>`)                                                                                             |
| Kênh         | **private**. RLS trên `realtime.messages`: mạch có `board_id` khớp được **đọc**; không ai ngoài server (`service_role`) được **ghi**; người dùng không đọc được |
| Join payload | `{"config":{"broadcast":{"self":false},"private":true},"access_token":"<boardToken>"}` (bỏ `presence`, 🔁)                                                              |
| Heartbeat    | Topic `phoenix`, mỗi **25s**. Quá **10s** không có trả lời thì mạch ngắt và kết nối lại                               |
| Kết nối lại  | Khoảng chờ tăng dần 2s → 4s → … → 60s, ±20% ngẫu nhiên                                                                |

Nếu join bị từ chối (sai token hoặc RLS), mạch ghi log và **không thử lại cho tới lần kết nối sau**.

### 3.2 Server → Mạch: `command` (payload ✅, người gửi 🔁)

**Server** (Edge Function `send-command`, mục 9) gửi broadcast `event = "command"` vào kênh của mạch. App không tự tạo payload này. Payload mạch nhận không đổi:

```json
{
    "commandId": "6f1c2a0e-6b7d-4a53-9a51-0d7f1b2c3d4e",
    "deviceId": "tv_living",
    "action": "POWER",
    "params": {},
    "expiresAt": 1760421234567,
    "seq": 812345678
}
```

| Trường      | Kiểu   | Bắt buộc | Quy tắc                                                                                                                                                                                                                                                                                       |
| ----------- | ------ | -------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `commandId` | string | ✅       | Server sinh (= `commands.id`), duy nhất, **≤ 40 ký tự** (UUID 36 ký tự là được). Gửi lại phải **giữ nguyên** `commandId`                                                                                                                                                                                        |
| `deviceId`  | string | ✅       | Khoá trong `devices` của profile **của mạch này** (mục 5). Server đổi từ `home_devices.id` sang khoá này                                                                                                                                                                                                                                                      |
| `action`    | string | ✅       | Tivi: khoá trong `commands` của thiết bị. Điều hòa: xem 3.3                                                                                                                                                                                                                                   |
| `params`    | object |          | Chỉ dùng cho điều hòa (3.3)                                                                                                                                                                                                                                                                   |
| `expiresAt` | number | ✅       | **Epoch milliseconds**. Đề xuất `Date.now() + 10000`. **Thiếu trường này thì lệnh bị coi là quá hạn ngay** (khi mạch đã có giờ NTP). Tính theo **giờ server**, không theo giờ điện thoại                                                                                                                                                           |
| `seq`       | number | ✅       | Số nguyên **0 … 4 294 967 295**, tăng dần. Chỉ dùng khi mạch **chưa có giờ NTP**: lúc đó mạch bỏ lệnh có `seq` ≤ lệnh gần nhất. Vượt khoảng thì bị coi như `0`. Server cấp từ **bộ đếm riêng của từng mạch** (VD cột `boards.last_seq`, tăng trong cùng transaction ghi `commands`). Không để app tự sinh: nhiều điện thoại đồng hồ lệch nhau sẽ làm mạch bỏ lệnh |

Mạch xử lý theo thứ tự:

| Tình huống                                    | Mạch làm gì                              | ACK                                                     |
| --------------------------------------------- | ---------------------------------------- | ------------------------------------------------------- |
| Thiếu `commandId` / `deviceId` / `action`     | Bỏ qua                                   | `ok:false, error:"E_BAD_REQUEST"`                       |
| `commandId` đã xử lý (trong 16 lệnh gần nhất) | **Không phát IR lại**                    | ACK lại **đúng kết quả lần đầu**, thêm `duplicate:true` |
| Quá hạn (`now > expiresAt`) hoặc `seq` cũ     | Bỏ qua                                   | **Không có ACK**                                        |
| Quá **5 lệnh / 1 giây**                       | Bỏ qua                                   | `ok:false, error:"E_RATE_LIMITED"` (gửi lại sau được)   |
| Hợp lệ                                        | Phát IR, phát âm thanh xác nhận trên loa | `ok:true` hoặc mã lỗi                                   |

**Thử lại** (theo spec 4.1): app chờ kết quả **3 giây**; không có thì gọi lại `send-command` với cùng `clientRequestId`, server broadcast lại **đúng `commandId` cũ** (mạch sẽ không phát IR lần 2). Server từ chối ngay nếu mạch offline (mục 3.6).

### 3.3 Lệnh cho điều hòa ✅

Thiết bị có `irac_protocol` trong profile được coi là điều hòa. Mỗi lệnh gửi **toàn bộ trạng thái**:

| `action`                                         | Ý nghĩa                  |
| ------------------------------------------------ | ------------------------ |
| `POWER_OFF`                                      | Tắt                      |
| Bất kỳ giá trị khác (đề xuất `SET` / `POWER_ON`) | Bật và đặt theo `params` |

| `params` | Kiểu          | Mặc định | Quy tắc                                                                                                                    |
| -------- | ------------- | -------- | -------------------------------------------------------------------------------------------------------------------------- |
| `power`  | boolean       | `true`   | `false` cũng làm tắt                                                                                                       |
| `temp`   | **số nguyên** | `24`     | Bị giới hạn trong `limits.temp_min…temp_max` của thiết bị (mặc định 16–30). Số thập phân (24.5) bị bỏ qua và dùng mặc định |
| `mode`   | string        | `"cool"` | `auto`, `cool`, `heat`, `dry`, `fan` (không phân biệt hoa thường). Giá trị lạ → `auto`                                     |
| `fan`    | string        | `"auto"` | `auto`, `min`, `low`, `medium`, `high`, `max`. Giá trị lạ → `auto`                                                         |

Mạch luôn cần **đủ** `temp`/`mode`/`fan`. App chỉ gửi phần thay đổi; **server** ghép với `device_state` hiện tại thành trạng thái đầy đủ rồi mới gửi xuống, và lưu lại khi ACK `ok` (nhiều người cùng điều khiển vẫn khớp, spec 4.4).

### 3.4 Mạch → Server: `ack` (payload ✅, đường gửi 🔁)

Mạch gọi RPC (thay cho broadcast `ack` hiện tại):

```
POST https://<project>.supabase.co/rest/v1/rpc/board_ack     (header như mục 4)
```

Hàm `board_ack` (security definer) kiểm tra `commandId` thuộc đúng `board_id` trong token, cập nhật `commands.status/error/acked_at`, cập nhật `device_state` nếu `ok`, rồi đẩy kết quả cho app (mục 9). Body là payload ACK:

```json
{ "commandId": "6f1c…", "ok": true }
{ "commandId": "6f1c…", "ok": false, "error": "E_CMD_NOT_FOUND" }
{ "commandId": "6f1c…", "ok": true, "duplicate": true }
```

| `error`             | Nghĩa                                                     | App nên làm                                                            |
| ------------------- | --------------------------------------------------------- | ---------------------------------------------------------------------- |
| `E_CMD_NOT_FOUND`   | Không có thiết bị / nút này trong profile trên mạch       | Kiểm tra `profileVersion` mạch đang có (mục 3.6); gợi ý tạo remote ảo |
| `E_IR_SEND_FAIL`    | Protocol không hỗ trợ hoặc thư viện báo lỗi khi phát      | Báo lỗi thiết bị                                                       |
| `E_PROFILE_INVALID` | Dữ liệu mã IR của nút bị hỏng (thiếu `code`, raw quá dài) | Sửa profile                                                            |
| `E_BUSY`            | Mạch đang học lệnh, không phát IR                         | Thử lại sau                                                            |
| `E_RATE_LIMITED`    | Gửi quá nhanh                                             | Thử lại sau ~1s                                                        |
| `E_BAD_REQUEST`     | Thiếu trường bắt buộc                                     | Lỗi phía app                                                           |

- ACK gửi **ngay**, không xếp hàng đợi lâu: thất bại thì thử lại vài lần trong thời hạn của lệnh rồi bỏ (lệnh đã quá hạn thì ACK không còn ý nghĩa). Giữ kết nối HTTPS (keep-alive) để ACK nhanh.
- Cột "App nên làm" ở trên là việc app làm khi **server chuyển** kết quả về.

Lưu ý: ACK chỉ cho biết mạch **đã phát IR**. IR là truyền một chiều, nên mạch không biết thiết bị có nhận được hay không.

### 3.5 Học lệnh từ app (gán phím remote ảo) (payload ✅, đường đi 🔁)

App gọi Edge Function `learn-start` / `learn-cancel` (mục 9). Server tạo dòng `learn_sessions` (`requestId` = id dòng), **chỉ cho một phiên đang chạy mỗi mạch** (có phiên khác → trả lỗi "đang có người gán phím", không gửi xuống mạch), rồi gửi xuống kênh của mạch:

Server → Mạch:

| Event          | Payload                                                          |
| -------------- | ---------------------------------------------------------------- |
| `learn_start`  | `{ "requestId": "r-123" }`. Mạch bật mắt thu, chờ tối đa **15s** |
| `learn_cancel` | `{}`                                                             |

Mạch → Server: gọi RPC `board_learn_event` với body `{ "event": "<tên event>", ...payload }` (thay cho broadcast hiện tại). Server cập nhật `learn_sessions` và chỉ chuyển cho **người mở phiên**. Mọi event đều mang lại `requestId`:

| Event            | Payload                                         | Khi nào                             |
| ---------------- | ----------------------------------------------- | ----------------------------------- |
| `learn_progress` | `{ "requestId", "status": "first_captured" }`   | Đã thu lần 1, mời bấm lại lần 2     |
| `learn_progress` | `{ "requestId", "status": "mismatch" }`         | 2 lần khác nhau, mời bấm lại từ đầu |
| `learn_result`   | `{ "requestId", "status": "ok", "ir": IrCode }` | Thành công (IrCode xem mục 6)       |
| `learn_result`   | `{ "requestId", "status": "E_LEARN_FAIL" }`     | Lệch quá 3 lần                      |
| `learn_timeout`  | `{ "requestId", "status": "timeout" }`          | Hết 15s                             |

Trong lúc đang học, mọi `command` sẽ nhận `E_BUSY`.

### 3.6 Trạng thái mạch (thay cho Presence, 🔁)

Mạch gọi RPC `board_status` khi vừa vào kênh, mỗi **5 phút**, và mỗi khi nguồn hoặc mức pin thay đổi. Server ghi vào `boards` và đặt `last_seen_at = now()`. Body giữ nguyên các trường đang gửi qua Presence:

```json
{
    "battery": 76,
    "charging": false,
    "usb": false,
    "rssi": -61,
    "firmware": "0.1.0",
    "profileVersion": 3,
    "pendingLearnedCodes": 0
}
```

| Trường                | Ý nghĩa                                                                                     |
| --------------------- | ------------------------------------------------------------------------------------------- |
| `battery`             | % pin (0–100). Đang sạc thì số này cao hơn thực tế                                          |
| `charging`            | Đang sạc (cắm USB và pin < 4.15V)                                                           |
| `usb`                 | Đang cắm nguồn                                                                              |
| `rssi`                | dBm. Dưới khoảng −75 thì app nên khuyên dời mạch gần router                                 |
| `profileVersion`      | Phiên bản profile mạch đang dùng. Khác `boards.profile_version` nghĩa là mạch chưa cập nhật |
| `pendingLearnedCodes` | Số mã học từ nút còn chờ gửi lên DB (do mất mạng)                                           |

**Online/offline**: server coi mạch offline khi `last_seen_at` cũ hơn ~**6 phút** (chu kỳ + dư). Mọi ACK/RPC khác của mạch cũng cập nhật `last_seen_at`. Muốn phát hiện offline nhanh hơn thì giảm chu kỳ (tốn pin hơn) — cần đo rồi chốt.

### 3.7 Cập nhật profile 🚧

Đề xuất: profile tính **theo từng mạch** (chỉ gồm thiết bị có `board_id` là mạch đó). Khi thiết bị của mạch thay đổi, server tăng `boards.profile_version` của mạch đó (đổi `board_id` thì tăng cả mạch cũ và mạch mới) và gửi broadcast `profile_updated { "profileVersion": 4 }` vào kênh của mạch. Mạch thấy khác phiên bản thì tải profile qua HTTPS rồi lưu lại. **Firmware chưa làm** phần này: hiện mạch chỉ đọc `profile.json` có sẵn trong bộ nhớ flash. Cần chốt endpoint tải profile (bảng/view hay Edge Function) và định dạng (mục 5).

---

## 4. REST: mạch ghi vào DB ✅

Chung cho mọi request:

```
POST https://<project>.supabase.co/rest/v1/<table>
apikey: <anon>
Authorization: Bearer <boardToken>
Content-Type: application/json
Prefer: return=minimal
```

- Mạch coi **chỉ HTTP 201** là thành công. Mọi mã khác thì giữ lại và **thử lại sau 30s**, gửi tuần tự từng bản ghi.
- Nếu mạch không nhận được 201 do mạng chập chờn, **một bản ghi có thể bị gửi 2 lần**. Backend nên chấp nhận hoặc tự lọc trùng.
- Hàng đợi chỉ nằm trong RAM: mất điện là mất. 🚧 Lưu xuống flash sẽ làm sau.

### 4.1 `learned_codes`: mã học từ nút trên mạch

Người dùng giữ nút 5s rồi bấm remote gốc. Mạch ghi mã vào bảng này, sau đó chủ nhà gán mã vào thiết bị/nút trong app (spec 7.1, "Cách 2"). Mất mạng thì mạch giữ tối đa **4 mã**; mã cũ nhất bị bỏ nếu đầy.

Body mạch gửi:

```json
{
    "home_id": "…",
    "board_id": "…",
    "learned_at_epoch": 1760421234,
    "ir": { "protocol": "NEC", "code": "0x20DF10EF", "bits": 32 }
}
```

| Cột                   | Kiểu đề xuất                 | Ghi chú                                                                                                |
| --------------------- | ---------------------------- | ------------------------------------------------------------------------------------------------------ |
| `id`                  | uuid, default                | Mạch không gửi                                                                                         |
| `home_id`, `board_id` | uuid/text                    | RLS **phải** kiểm tra khớp với claim của token                                                         |
| `learned_at_epoch`    | bigint, **nullable**         | Epoch **giây**. **Không có trường này** khi mạch chưa có giờ NTP lúc học; khi đó dùng thời điểm insert |
| `ir`                  | jsonb                        | IrCode (mục 6)                                                                                         |
| `status`              | text, default `'pending'`    | Mạch không gửi. App đổi thành `assigned` / `discarded`                                                 |
| `created_at`          | timestamptz, default `now()` |                                                                                                        |

### 4.2 `error_reports`: báo lỗi

Mạch đã lọc chống spam trước khi gửi: chỉ gửi khi cùng một lỗi trên cùng một thiết bị lặp **≥ 3 lần trong 24h**, hoặc khi người dùng tự báo; và mỗi cặp lỗi + thiết bị gửi **tối đa 1 lần/giờ**.

```json
{
    "home_id": "…",
    "board_id": "…",
    "code": "E_CMD_NOT_FOUND",
    "device_id": "tv_living",
    "occurred_at_epoch": 1760421234,
    "firmware": "0.1.0"
}
```

| Cột                 | Ghi chú                                                                                                                             |
| ------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| `code`              | `E_PROFILE_INVALID`, `E_CMD_NOT_FOUND`, `E_IR_SEND_FAIL`, `E_USER_REPORTED`, `E_LEARN_FAIL`, `E_LOW_BATTERY`                        |
| `device_id`         | **Không có trường này** với lỗi không gắn thiết bị (`E_LEARN_FAIL`, `E_LOW_BATTERY`)                                                |
| `occurred_at_epoch` | Epoch giây. **Giá trị < 1 700 000 000 nghĩa là mạch chưa có giờ** (đó là số giây kể từ lúc khởi động), khi đó dùng thời điểm insert |

Báo cáo không chứa tên WiFi, mật khẩu hay thông tin cá nhân.

---

## 5. Profile của mạch (bản sao trên mạch)

Mạch tra mã IR **trong bộ nhớ của chính nó**, không hỏi cloud mỗi lần nhận lệnh. Mỗi mạch chỉ giữ thiết bị **mạch đó phát IR tới**, không phải cả nhà. Định dạng mạch đọc ✅ (ví dụ đầy đủ ở [data/profile.json](../data/profile.json)):

```json
{
    "profileVersion": 3,
    "devices": {
        "tv_living": {
            "type": "tv",
            "name": "Tivi phòng khách",
            "commands": {
                "POWER": {
                    "protocol": "SAMSUNG",
                    "code": "0xE0E040BF",
                    "bits": 32
                },
                "MOVIE_MODE": { "raw": [9000, 4500, 560, 560], "freq": 38 }
            }
        },
        "ac_living": {
            "type": "ac",
            "irac_protocol": "PANASONIC_AC",
            "limits": { "temp_min": 16, "temp_max": 30 }
        }
    }
}
```

| Quy tắc          |                                                                                                |
| ---------------- | ---------------------------------------------------------------------------------------------- |
| `profileVersion` | **Bắt buộc**, số nguyên ≥ 0. Thiếu thì mạch từ chối cả profile (`E_PROFILE_INVALID`)           |
| `devices`        | **Bắt buộc**, object; khoá chính là `deviceId` dùng trong `command`. Chỉ cần duy nhất trong một mạch |
| Điều hòa         | Có `irac_protocol` (tên protocol IRac). Không cần `commands`                                   |
| Thiết bị bấm nút | `commands`: khoá = `action` (POWER, VOL_UP, … hoặc khoá tuỳ ý của remote ảo), giá trị = IrCode |
| `name`, `type`   | Mạch không dùng, chỉ để người đọc                                                              |
| Kích thước       | Nên giữ **< 32KB** (mạch giữ toàn bộ profile trong RAM)                                        |

`voiceAliases`, icon, nhãn nút… là việc của app/NLP, **không cần gửi xuống mạch**.

---

## 6. IrCode: định dạng mã IR (dùng chung)

Hai dạng, dùng ở profile, `learn_result` và `learned_codes.ir`:

```json
{ "protocol": "NEC", "code": "0x20DF10EF", "bits": 32 }
{ "protocol": "UNKNOWN", "raw": [9000, 4500, 560, 560, 560, 1690], "freq": 38 }
```

| Trường     | Quy tắc                                                                                                                                                    |
| ---------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `protocol` | Tên protocol theo thư viện **IRremoteESP8266** (`NEC`, `SAMSUNG`, `SONY`, `LG`, `PANASONIC`, …), viết hoa. `UNKNOWN` = mạch không giải mã được, dùng `raw` |
| `code`     | **Chuỗi hex** có tiền tố `0x`. Không dùng number: JS mất chính xác với số 64 bit                                                                           |
| `bits`     | Số bit của mã                                                                                                                                              |
| `raw`      | Mảng thời lượng µs (xen kẽ có xung / không xung), mỗi phần tử ≤ 65535, **≤ 600 phần tử**                                                                   |
| `freq`     | kHz, mặc định 38                                                                                                                                           |

Trong profile, nút có `raw` được ưu tiên dùng `raw` (không cần `protocol`).

---

## 7. Ghép đôi 🚧

Firmware đã có phần hiển thị: mạch hiện **tên WiFi riêng, mật khẩu ngẫu nhiên và mã 6 số** trên OLED. Mã hết hạn sau **10 phút**. Mạch mới hoặc vừa reset tự vào chế độ này; ngoài ra giữ nút 10s cũng vào được. Phần nói chuyện với backend **chưa làm**, đề xuất như sau:

```
Mạch  → POST /functions/v1/board-register   { boardId, pairingCode }   (chỉ anon key)
App   → POST /functions/v1/pair-board        { homeId, pairingCode }    (JWT người dùng, phải là owner)
Mạch  → hỏi lại board-register cho tới khi nhận { boardToken, homeId }
```

Nhà đã có mạch thì ghép thêm mạch mới vào **cùng nhà** (không tạo nhà mới). Yêu cầu phía Edge Function: lưu **hash** của mã, mỗi mã dùng 1 lần, hết hạn 10 phút, khoá sau vài lần nhập sai. Mạch đã ghép đôi thì không cho ghép lại nếu chưa reset. Chi tiết luồng đăng nhập Zalo → JWT xem ghi chú trong spec mục 10.

---

## 8. Thử nhanh không cần backend

Firmware nhận cùng các message ở mục 3.2 và 3.5 qua **cổng Serial** (115200 baud, mỗi message một dòng JSON):

```
{"event":"command","payload":{"commandId":"c1","deviceId":"tv_living","action":"POWER","seq":1,"expiresAt":1760421234567}}
```

Kết quả hiện trong log (`ACK c1: ok`). Dùng để kiểm tra payload trước khi nối Supabase. Bản phát hành sẽ tắt tính năng này.

---

## 9. App ↔ Server (Edge Functions) 🚧

Không phải việc của firmware, ghi ở đây để app và server khớp với mục 3. Mọi hàm nhận **JWT người dùng** và kiểm tra quyền **ở mỗi lần gọi** (thuộc nhà, vai trò, sau này là quyền từng thiết bị).

| Hàm                           | App gửi                                                                       | Server làm                                                                                                                          |
| ----------------------------- | ----------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| `send-command`                | `{ deviceId (home_devices.id), action, params?, clientRequestId }`            | Tìm `board_id`; mạch offline → trả lỗi ngay; ghép `device_state` (điều hòa); ghi `commands` kèm `user_id`; sinh `commandId`/`seq`/`expiresAt`; broadcast (mục 3.2). Trùng `clientRequestId` → gửi lại `commandId` cũ. Giới hạn tốc độ theo người và theo mạch |
| `learn-start`, `learn-cancel` | `{ boardId }` / `{ requestId }`                                               | Chỉ owner; một phiên mỗi mạch; ghi `learn_sessions`; broadcast (mục 3.5)                                                            |
| `pair-board`                  | `{ homeId, pairingCode }`                                                     | Mục 7                                                                                                                               |

Kết quả về app: app nghe kênh `home:<homeId>` (private, thành viên chỉ đọc) — server/RPC của mạch gửi `command_result` (cho người ra lệnh, kèm `userId`), `device_state` (cả nhà), `board_status` (cả nhà), `learn_*` (chỉ người mở phiên, có thể dùng kênh riêng `user:<userId>`). Có thể thay bằng Postgres Changes trên `commands`/`device_state`/`boards` với RLS; chốt một cách khi làm backend.

