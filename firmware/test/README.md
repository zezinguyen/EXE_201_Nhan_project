# Unit test

Mỗi thư mục `test_*` là một bộ test Unity độc lập cho một lớp trong `lib/core`.

```bash
pio test -e native                     # chạy toàn bộ trên máy tính
pio test -e native -f test_backoff     # chạy một bộ
pio test -e esp32dev -f test_backoff   # chạy trên mạch thật (cần cắm board)
```

File test dùng mẫu `#ifdef ARDUINO ... setup()/loop() ... #else main()` để chạy được cả hai nơi.
Chỉ test code trong `lib/core` (không phụ thuộc phần cứng). Logic mới nên tách vào `lib/core` để test được.
