#pragma once
#include <stdint.h>

// Số thứ tự file âm thanh trên thẻ nhớ DFPlayer: /mp3/0001.mp3, /mp3/0002.mp3, ...
// (DFPlayer chỉ tìm đúng file theo số khi đặt trong thư mục mp3 và tên 4 chữ số)
enum class AudioTrack : uint16_t {
    CommandOk = 1,        // "Đã nhận lệnh"
    CommandNotFound = 2,  // "Không tìm thấy lệnh"
    SetupMode = 3,        // "Đang vào chế độ thiết lập"
    LearnMode = 4,        // "Mời bấm phím trên remote"
    LearnSuccess = 5,     // "Đã học lệnh"
    LearnFailed = 6,      // "Học lệnh thất bại"
    CommandFailed = 7,    // "Không thực hiện được lệnh" (phát IR lỗi, đang bận, quá nhiều lệnh)
    LowBattery = 8,       // "Pin yếu"
    FactoryReset = 9,     // "Khôi phục cài đặt gốc"
};

// Chọn âm thanh cho kết quả một lệnh. error = nullptr nghĩa là thành công.
AudioTrack audioTrackForCommandResult(const char* error);
