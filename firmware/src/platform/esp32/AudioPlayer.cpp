#include "modules/AudioPlayer.h"

#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>

#include "config.h"

namespace {
HardwareSerial audioSerial(AUDIO_UART_NUM);
DFRobotDFPlayerMini dfPlayer;


// Thư viện begin(..., doReset = false) luôn trả true, nên phải hỏi thử để biết DFPlayer có thật.
// Mỗi câu hỏi chờ tối đa ~500ms, chỉ chạy một lần lúc khởi động.
AudioFault probeDfPlayer() {
    if (dfPlayer.readState() < 0) {
        return AudioFault::NoResponse;
    }
    if (dfPlayer.readFileCounts(DFPLAYER_DEVICE_SD) <= 0) {
        return AudioFault::NoCard;
    }
    return AudioFault::None;
}
}  // namespace

void AudioPlayer::begin() {
    state_ = State::Booting;
    fault_ = AudioFault::None;
    stateStartMs_ = millis();
}

void AudioPlayer::update(uint32_t nowMs) {
    switch (state_) {
        case State::Booting:
            if (nowMs - stateStartMs_ < AUDIO_BOOT_DELAY_MS) {
                return;
            }
            audioSerial.begin(AUDIO_BAUD, SERIAL_8N1, PIN_AUDIO_RX, PIN_AUDIO_TX);
            dfPlayer.begin(audioSerial, true, false);
            fault_ = probeDfPlayer();
            if (fault_ != AudioFault::None) {
                log_e("%s", fault_ == AudioFault::NoResponse ? "DFPlayer khong tra loi UART (day TX/RX, nguon 3V3)"
                                                             : "DFPlayer khong thay file (the nho?)");
                state_ = State::Failed;
                return;
            }
            dfPlayer.volume(AUDIO_VOLUME);
            state_ = State::Idle;
            if (pending_) {
                pending_ = false;
                playNow(lastTrack_, nowMs);
            }
            break;
        case State::Playing:
            if ((dfPlayer.available() && dfPlayer.readType() == DFPlayerPlayFinished) ||
                nowMs - stateStartMs_ >= AUDIO_MAX_PLAY_MS) {
                state_ = State::Idle;
            }
            break;
        case State::Idle:
        case State::Failed:
            break;
    }
}

void AudioPlayer::play(AudioTrack track) {
    lastTrack_ = track;
    if (disabled_) {
        return;
    }
    if (state_ == State::Booting) {
        pending_ = true;  // Phát ngay khi DFPlayer sẵn sàng
    } else if (isReady()) {
        playNow(track, millis());
    }
}

void AudioPlayer::stop() {
    pending_ = false;
    if (state_ == State::Playing) {
        dfPlayer.stop();
        state_ = State::Idle;
    }
}

void AudioPlayer::setDisabled(bool disabled) {
    disabled_ = disabled;
    if (disabled_) {
        stop();
    }
}

void AudioPlayer::playNow(AudioTrack track, uint32_t nowMs) {
    dfPlayer.playMp3Folder(static_cast<uint16_t>(track));
    state_ = State::Playing;
    stateStartMs_ = nowMs;
}
