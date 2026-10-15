#include "Feedback.h"

void Feedback::onLearnEvent(LearnEvent event) {
    switch (event) {
        case LearnEvent::Success:
            audio_.play(AudioTrack::LearnSuccess);
            break;
        case LearnEvent::Failed:
        case LearnEvent::Timeout:
            audio_.play(AudioTrack::LearnFailed);
            break;
        case LearnEvent::None:
        case LearnEvent::FirstCaptured:
        case LearnEvent::Mismatch:
            break;
    }
}
