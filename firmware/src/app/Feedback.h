#pragma once
#include <AudioTracks.h>

#include "modules/AudioPlayer.h"
#include "modules/IrLearner.h"

// Phản hồi bằng âm thanh cho người đứng cạnh mạch. Mọi chỗ cần phát tiếng đi qua đây
// để bảng "sự kiện -> file mp3" nằm một chỗ (xem AudioTrack).
class Feedback {
public:
    explicit Feedback(AudioPlayer& audio) : audio_(audio) {}

    void onCommandResult(const char* error) { audio_.play(audioTrackForCommandResult(error)); }
    void onSetupMode() { audio_.play(AudioTrack::SetupMode); }
    void onLearnStart() { audio_.play(AudioTrack::LearnMode); }
    void onLearnEvent(LearnEvent event);
    void onLowBattery() { audio_.play(AudioTrack::LowBattery); }
    void onFactoryReset() { audio_.play(AudioTrack::FactoryReset); }

private:
    AudioPlayer& audio_;
};
