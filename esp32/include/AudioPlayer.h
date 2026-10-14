#pragma once
#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>
#include "config.h"
#include "AudioTracks.h"

class AudioPlayer {
public:
    static AudioPlayer& instance() {
        static AudioPlayer player;
        return player;
    }

    void begin();
    void playTrack(AudioTrack track);
    void setVolume(uint8_t volume);

private:
    AudioPlayer() = default;
    DFRobotDFPlayerMini dfPlayer;
    HardwareSerial dfSerial{2};
    bool isAvailable = false;
};

