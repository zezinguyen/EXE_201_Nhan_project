#pragma once
#include <AudioTracks.h>
#include <stdint.h>

enum class AudioFault : uint8_t {
    None,
    NoResponse,  // Không trả lời qua UART: dây TX/RX, nguồn
    NoCard,      // Trả lời nhưng không thấy file: thẻ nhớ chưa cắm / rỗng / sai định dạng
};

// DFPlayer Mini (nguồn 3V3 thường trực) -> DAC -> PAM8403 -> loa.
// DFPlayer cần ~1.5s sau khi cấp điện để đọc thẻ nhớ: begin() không chặn,
// lệnh play() trong lúc đó được giữ lại và phát ngay khi sẵn sàng.
// Triển khai riêng theo MCU (UART phần cứng): src/platform/<mcu>/AudioPlayer.cpp
class AudioPlayer {
public:
    void begin();
    void update(uint32_t nowMs);
    void play(AudioTrack track);
    void stop();

    bool isReady() const { return state_ == State::Idle || state_ == State::Playing; }
    bool isBusy() const { return state_ == State::Playing; }
    bool hasFailed() const { return state_ == State::Failed; }
    AudioFault fault() const { return fault_; }
    // Track được yêu cầu gần nhất (kể cả khi đang tắt tiếng) – để test kiểm tra
    AudioTrack lastTrack() const { return lastTrack_; }
    // Pin yếu: không phát nữa
    void setDisabled(bool disabled);

private:
    enum class State : uint8_t { Booting, Idle, Playing, Failed };

    void playNow(AudioTrack track, uint32_t nowMs);

    State state_ = State::Booting;
    AudioFault fault_ = AudioFault::None;
    AudioTrack lastTrack_ = AudioTrack::CommandOk;
    bool pending_ = false;
    bool disabled_ = false;
    uint32_t stateStartMs_ = 0;
};
