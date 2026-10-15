// Test trên mạch thật: mô phỏng request từ app, kiểm tra mạch xử lý và phát đúng âm thanh.
// Cần: DFPlayer + thẻ nhớ (/mp3/0001.mp3 ...) + PAM8403 + loa. Đứng cạnh mạch để nghe.
// Chạy: pio test -e esp32dev -f test_device_feedback

#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>
#include <unity.h>

#include "app/CommandHandler.h"
#include "app/Feedback.h"
#include "app/Outbox.h"
#include "config.h"
#include "modules/AudioPlayer.h"
#include "modules/Board.h"
#include "modules/DeviceStore.h"
#include "modules/ErrorReporter.h"
#include "modules/IrLearner.h"
#include "modules/IrSender.h"
#include "modules/RealtimeClient.h"
#include "modules/SecureStore.h"
#include "modules/SupabaseRest.h"
#include "modules/TimeSync.h"
#include "modules/WifiManager.h"

namespace {
const char kProfile[] =
    R"({"profileVersion":1,"devices":{"tv_test":{"type":"tv","commands":{)"
    R"("POWER":{"protocol":"NEC","code":"0x20DF10EF","bits":32}}}}})";

SecureStore secureStore;
DeviceStore store;
IrSender sender;
IrLearner learner;
ErrorReporter errors;
TimeSync timeSync;
RealtimeClient realtime;  // Không kết nối: ACK chỉ ra log
SupabaseRest rest;
Outbox outbox(rest, errors);
CommandHandler handler(store, sender, learner, errors, timeSync, realtime, outbox);
AudioPlayer audio;
Feedback feedback(audio);
WifiManager wifi;

uint32_t nextSeq = 1;
bool resultReceived = false;
char lastError[32] = {};  // "" = thành công

void onResult(const char* /*commandId*/, const char* error) {
    resultReceived = true;
    strncpy(lastError, error != nullptr ? error : "", sizeof(lastError) - 1);
    feedback.onCommandResult(error);
}

// Gửi request "command" giống app gửi qua Realtime
void sendCommand(const char* commandId, const char* deviceId, const char* action) {
    JsonDocument doc;
    doc["commandId"] = commandId;
    doc["deviceId"] = deviceId;
    doc["action"] = action;
    doc["seq"] = nextSeq++;
    doc["expiresAt"] = timeSync.nowEpochMs() + 10000;
    resultReceived = false;
    lastError[0] = '\0';
    handler.onBroadcast("command", doc.as<JsonObjectConst>());
}

// Cho DFPlayer phát hết câu để người test nghe được, rồi nghỉ một chút giữa các câu
void playAndWait() {
    const uint32_t start = millis();
    do {
        audio.update(millis());
        delay(LOOP_IDLE_MS);
    } while (audio.isBusy() && millis() - start < AUDIO_MAX_PLAY_MS + 500);
    delay(500);
}

void waitAudioReady() {
    const uint32_t start = millis();
    while (!audio.isReady() && !audio.hasFailed() && millis() - start < AUDIO_BOOT_DELAY_MS + 3000) {
        audio.update(millis());
        delay(LOOP_IDLE_MS);
    }
}
}  // namespace

void setUp() {}
void tearDown() {}

void testDfPlayerResponds() {
    waitAudioReady();
    TEST_ASSERT_TRUE_MESSAGE(audio.isReady(), "DFPlayer khong phan hoi: kiem tra TX/RX, nguon 3V3, the nho");
}

void testKnownCommandPlaysTrack1() {
    sendCommand("t-ok", "tv_test", "POWER");
    TEST_ASSERT_TRUE(resultReceived);
    TEST_ASSERT_EQUAL_STRING("", lastError);
    TEST_ASSERT_EQUAL(AudioTrack::CommandOk, audio.lastTrack());
    playAndWait();  // Nghe: 0001.mp3 (đồng thời LED IR phát mã NEC)
}

void testUnknownActionPlaysTrack2() {
    sendCommand("t-no-action", "tv_test", "NOT_A_BUTTON");
    TEST_ASSERT_TRUE(resultReceived);
    TEST_ASSERT_EQUAL_STRING("E_CMD_NOT_FOUND", lastError);
    TEST_ASSERT_EQUAL(AudioTrack::CommandNotFound, audio.lastTrack());
    playAndWait();  // Nghe: 0002.mp3
}

void testUnknownDevicePlaysTrack2() {
    sendCommand("t-no-device", "khong_co_thiet_bi", "POWER");
    TEST_ASSERT_TRUE(resultReceived);
    TEST_ASSERT_EQUAL_STRING("E_CMD_NOT_FOUND", lastError);
    TEST_ASSERT_EQUAL(AudioTrack::CommandNotFound, audio.lastTrack());
    playAndWait();
}

void testDuplicateCommandIsNotReplayed() {
    sendCommand("t-ok", "tv_test", "POWER");  // Trùng commandId của testKnownCommandPlaysTrack1
    TEST_ASSERT_FALSE_MESSAGE(resultReceived, "Lenh trung khong duoc thuc thi lai");
}

void testSetupModePlaysTrack3() {
    wifi.startSetupPortal(millis());
    TEST_ASSERT_TRUE(wifi.isSetupPortalActive());
    TEST_ASSERT_NOT_NULL(wifi.setupPassword());
    TEST_ASSERT_EQUAL_size_t(WIFI_SETUP_AP_PASSWORD_LENGTH, strlen(wifi.setupPassword()));
    feedback.onSetupMode();
    TEST_ASSERT_EQUAL(AudioTrack::SetupMode, audio.lastTrack());
    playAndWait();  // Nghe: 0003.mp3; điện thoại thấy WiFi "SmartHome-XXXX"
    wifi.stopSetupPortal();
}

void testLearnModePlaysTrack4() {
    handler.startLearning(LearnSource::Button, millis());
    feedback.onLearnStart();
    TEST_ASSERT_TRUE(handler.isLearning());
    TEST_ASSERT_EQUAL(AudioTrack::LearnMode, audio.lastTrack());
    playAndWait();  // Nghe: 0004.mp3
    learner.stop();
}

void testCommandWhileLearningPlaysTrack7() {
    handler.startLearning(LearnSource::Button, millis());
    sendCommand("t-busy", "tv_test", "POWER");
    learner.stop();
    TEST_ASSERT_TRUE(resultReceived);
    TEST_ASSERT_EQUAL_STRING("E_BUSY", lastError);
    TEST_ASSERT_EQUAL(AudioTrack::CommandFailed, audio.lastTrack());
    playAndWait();  // Nghe: 0007.mp3
}

void testLearnTimeoutPlaysTrack6() {
    // KHÔNG bấm remote trong ~15 giây
    handler.startLearning(LearnSource::Button, millis());
    LearnEvent event = LearnEvent::None;
    const uint32_t start = millis();
    while (event == LearnEvent::None && millis() - start < IR_LEARN_TIMEOUT_MS + 2000) {
        event = handler.update(millis());
        delay(LOOP_IDLE_MS);
    }
    TEST_ASSERT_EQUAL(LearnEvent::Timeout, event);
    TEST_ASSERT_FALSE(handler.isLearning());
    feedback.onLearnEvent(event);
    TEST_ASSERT_EQUAL(AudioTrack::LearnFailed, audio.lastTrack());
    playAndWait();  // Nghe: 0006.mp3
}

void setup() {
    delay(2000);  // Chờ cổng Serial của bộ chạy test
    boardBegin();
    secureStore.begin();
    TEST_ASSERT_TRUE(store.loadFromJson(kProfile, strlen(kProfile)));
    sender.begin();
    learner.begin();
    audio.begin();
    wifi.begin(secureStore);
    handler.setResultHandler(onResult);

    UNITY_BEGIN();
    RUN_TEST(testDfPlayerResponds);
    RUN_TEST(testKnownCommandPlaysTrack1);
    RUN_TEST(testUnknownActionPlaysTrack2);
    RUN_TEST(testUnknownDevicePlaysTrack2);
    RUN_TEST(testDuplicateCommandIsNotReplayed);
    RUN_TEST(testSetupModePlaysTrack3);
    RUN_TEST(testLearnModePlaysTrack4);
    RUN_TEST(testCommandWhileLearningPlaysTrack7);
    RUN_TEST(testLearnTimeoutPlaysTrack6);
    UNITY_END();
}

void loop() {}
