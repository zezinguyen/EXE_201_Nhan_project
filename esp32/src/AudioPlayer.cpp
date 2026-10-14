#include "AudioPlayer.h"

void AudioPlayer::begin() {
    // Khởi tạo UART2 nối DFPlayer Mini (TX2=GPIO17, RX2=GPIO16)
    dfSerial.begin(9600, SERIAL_8N1, PIN_DFPLAYER_RX, PIN_DFPLAYER_TX);
    delay(100);

    Serial.println("[AUDIO] Đang kết nối DFPlayer Mini...");
    if (dfPlayer.begin(dfSerial, /*isACK=*/false, /*doReset=*/true)) {
        Serial.println("[AUDIO] DFPlayer Mini khởi động thành công!");
        isAvailable = true;
        dfPlayer.volume(26); // Mức âm lượng chuẩn (0 - 30)
        dfPlayer.EQ(DFPLAYER_EQ_NORMAL);
    } else {
        Serial.println("[AUDIO] CẢNH BÁO: Không phát hiện thẻ nhớ hoặc DFPlayer Mini!");
        isAvailable = false;
    }
}

void AudioPlayer::playTrack(AudioTrack track) {
    if (!isAvailable) {
        Serial.printf("[AUDIO] Bỏ qua phát track %u (DFPlayer không sẵn sàng)\n", (uint16_t)track);
        return;
    }

    Serial.printf("[AUDIO] Phát file: /mp3/%04u.mp3\n", (uint16_t)track);
    // DFPlayer phát file theo số trong thư mục /mp3/
    dfPlayer.playMp3Folder((uint16_t)track);
}

void AudioPlayer::setVolume(uint8_t volume) {
    if (isAvailable && volume <= 30) {
        dfPlayer.volume(volume);
    }
}

