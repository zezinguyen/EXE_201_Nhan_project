#include <ErrorThrottle.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

static constexpr uint32_t kDay = 24 * 3600;

void testReportsAfterThreshold() {
    ErrorThrottle throttle(3, kDay, 3600);
    const uint32_t tv = hashKey("tv_1");
    TEST_ASSERT_FALSE(throttle.shouldReport(ErrorCode::IrSendFail, tv, 100, false));
    TEST_ASSERT_FALSE(throttle.shouldReport(ErrorCode::IrSendFail, tv, 200, false));
    TEST_ASSERT_TRUE(throttle.shouldReport(ErrorCode::IrSendFail, tv, 300, false));
}

void testUserReportSendsImmediately() {
    ErrorThrottle throttle(3, kDay, 3600);
    TEST_ASSERT_TRUE(throttle.shouldReport(ErrorCode::UserReported, hashKey("ac_1"), 100, true));
}

void testResendLimitedToOncePerHour() {
    ErrorThrottle throttle(1, kDay, 3600);
    const uint32_t tv = hashKey("tv_1");
    TEST_ASSERT_TRUE(throttle.shouldReport(ErrorCode::CmdNotFound, tv, 0, false));
    TEST_ASSERT_FALSE(throttle.shouldReport(ErrorCode::CmdNotFound, tv, 1800, false));
    TEST_ASSERT_TRUE(throttle.shouldReport(ErrorCode::CmdNotFound, tv, 3600, false));
}

void testDevicesAreIndependent() {
    ErrorThrottle throttle(1, kDay, 3600);
    TEST_ASSERT_TRUE(throttle.shouldReport(ErrorCode::CmdNotFound, hashKey("tv_1"), 0, false));
    TEST_ASSERT_TRUE(throttle.shouldReport(ErrorCode::CmdNotFound, hashKey("tv_2"), 0, false));
    TEST_ASSERT_TRUE(throttle.shouldReport(ErrorCode::IrSendFail, hashKey("tv_1"), 0, false));
}

void testWindowResetsCount() {
    ErrorThrottle throttle(3, kDay, 3600);
    const uint32_t tv = hashKey("tv_1");
    throttle.shouldReport(ErrorCode::IrSendFail, tv, 0, false);
    throttle.shouldReport(ErrorCode::IrSendFail, tv, 10, false);
    // Lần thứ 3 rơi vào ngày khác -> đếm lại từ 1
    TEST_ASSERT_FALSE(throttle.shouldReport(ErrorCode::IrSendFail, tv, kDay + 20, false));
}

void testEvictsOldestWhenFull() {
    ErrorThrottle throttle(1, kDay, 3600);
    char id[8] = "dev_00";
    for (uint8_t i = 0; i < ErrorThrottle::kSlots + 1; i++) {
        id[4] = static_cast<char>('a' + i);
        TEST_ASSERT_TRUE(throttle.shouldReport(ErrorCode::IrSendFail, hashKey(id), i, false));
    }
}

void testHashKeyDiffers() {
    TEST_ASSERT_NOT_EQUAL(hashKey("tv_1"), hashKey("tv_2"));
    TEST_ASSERT_EQUAL_UINT32(hashKey("abc"), hashKey("abc"));
}

void testErrorCodeNameRoundTrip() {
    ErrorCode code;
    TEST_ASSERT_TRUE(errorCodeFromName("E_IR_SEND_FAIL", code));
    TEST_ASSERT_EQUAL(ErrorCode::IrSendFail, code);
    TEST_ASSERT_TRUE(errorCodeFromName(errorCodeName(ErrorCode::LowBattery), code));
    TEST_ASSERT_EQUAL(ErrorCode::LowBattery, code);
    TEST_ASSERT_FALSE(errorCodeFromName("E_BUSY", code));
    TEST_ASSERT_FALSE(errorCodeFromName(nullptr, code));
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testReportsAfterThreshold);
    RUN_TEST(testUserReportSendsImmediately);
    RUN_TEST(testResendLimitedToOncePerHour);
    RUN_TEST(testDevicesAreIndependent);
    RUN_TEST(testWindowResetsCount);
    RUN_TEST(testEvictsOldestWhenFull);
    RUN_TEST(testHashKeyDiffers);
    RUN_TEST(testErrorCodeNameRoundTrip);
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
