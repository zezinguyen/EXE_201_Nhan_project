#include "AudioTracks.h"

#include <string.h>

#include "ErrorCodes.h"

AudioTrack audioTrackForCommandResult(const char* error) {
    if (error == nullptr) {
        return AudioTrack::CommandOk;
    }
    if (strcmp(error, errorCodeName(ErrorCode::CmdNotFound)) == 0) {
        return AudioTrack::CommandNotFound;
    }
    return AudioTrack::CommandFailed;
}
