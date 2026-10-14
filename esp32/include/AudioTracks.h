#pragma once
#include <Arduino.h>

// Bảng mã bài hát phát trên thẻ nhớ DFPlayer Mini (Spec 3.1)
// Cần copy các file vào thư mục /mp3/ với định dạng 0001.mp3 - 0009.mp3
enum AudioTrack : uint16_t {
    TRACK_CMD_SUCCESS       = 1, // 0001.mp3 - "Đã nhận lệnh" (thực hiện xong lệnh IR)
    TRACK_CMD_NOT_FOUND     = 2, // 0002.mp3 - "Không tìm thấy lệnh" (E_CMD_NOT_FOUND)
    TRACK_ENTER_SETUP       = 3, // 0003.mp3 - "Đang vào chế độ thiết lập" (WiFi AP + Mã ghép đôi)
    TRACK_PROMPT_REMOTE     = 4, // 0004.mp3 - "Mời bấm phím trên remote" (Bắt đầu học lệnh)
    TRACK_LEARN_SUCCESS     = 5, // 0005.mp3 - "Đã học lệnh" (Học lệnh thành công)
    TRACK_LEARN_FAIL        = 6, // 0006.mp3 - "Học lệnh thất bại" (Lệch quá 3 lần hoặc hết 15s)
    TRACK_CMD_FAILED        = 7, // 0007.mp3 - "Không thực hiện được lệnh" (E_IR_SEND_FAIL, E_BUSY...)
    TRACK_LOW_BATTERY       = 8, // 0008.mp3 - "Pin yếu" (Khi pin <= 20%)
    TRACK_FACTORY_RESET     = 9  // 0009.mp3 - "Khôi phục cài đặt gốc" (Giữ nút 20s)
};

