#include <AudioTracks.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void testTrackNumbersMatchSdCard() {
    TEST_ASSERT_EQUAL_UINT16(1, static_cast<uint16_t>(AudioTrack::CommandOk));
    TEST_ASSERT_EQUAL_UINT16(2, static_cast<uint16_t>(AudioTrack::CommandNotFound));
    TEST_ASSERT_EQUAL_UINT16(3, static_cast<uint16_t>(AudioTrack::SetupMode));
}

void testSuccessPlaysCommandOk() {
    TEST_ASSERT_EQUAL(AudioTrack::CommandOk, audioTrackForCommandResult(nullptr));
}

void testNotFoundPlaysCommandNotFound() {
    TEST_ASSERT_EQUAL(AudioTrack::CommandNotFound, audioTrackForCommandResult("E_CMD_NOT_FOUND"));
}

void testOtherErrorsPlayCommandFailed() {
    TEST_ASSERT_EQUAL(AudioTrack::CommandFailed, audioTrackForCommandResult("E_IR_SEND_FAIL"));
    TEST_ASSERT_EQUAL(AudioTrack::CommandFailed, audioTrackForCommandResult("E_BUSY"));
    TEST_ASSERT_EQUAL(AudioTrack::CommandFailed, audioTrackForCommandResult("E_RATE_LIMITED"));
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testTrackNumbersMatchSdCard);
    RUN_TEST(testSuccessPlaysCommandOk);
    RUN_TEST(testNotFoundPlaysCommandNotFound);
    RUN_TEST(testOtherErrorsPlayCommandFailed);
    return UNITY_END();
}

#ifdef ARDUINO
#include <Arduino.h>
void setup() {
    delay(2000);
    runTests();
}
void loop() {}
#else
int main() { return runTests(); }
#endif
