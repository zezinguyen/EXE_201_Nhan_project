# CLAUDE.md

Monorepo SmartHome Voice Assistant (Zalo Mini App + Supabase + mạch ESP32 phát IR).

| Thư mục | Nội dung |
|---|---|
| `docs/` | Tài liệu chung: `specs.md` (đặc tả), `contracts.md` (giao tiếp app ↔ server ↔ mạch), `production-notes.md` |
| `firmware/` | Firmware PlatformIO cho mạch — quy tắc riêng ở [firmware/CLAUDE.md](firmware/CLAUDE.md), mọi lệnh `pio` chạy trong thư mục này |
| `zalo-app/` | Zalo Mini App (React + Vite) |
| `database/` | Schema Supabase (SQL) |
| `spring-backend/` | Backend Spring Boot dự phòng |

Đổi giao tiếp giữa các phần (payload Realtime, RPC, profile, mã IR) thì sửa `docs/contracts.md` cùng lúc.
