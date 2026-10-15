#include <CommandGuard.h>
#include <RateLimiter.h>
#include <unity.h>

#include <stdio.h>

void setUp() {}
void tearDown() {}

static constexpr int64_t kNow = 1760000000000LL;

void testAcceptsFreshCommand() {
    CommandGuard guard(5, 1000);
    TEST_ASSERT_EQUAL(CommandVerdict::Accept, guard.check("cmd-1", kNow + 10000, 1, kNow, true, 0));
}

void testRejectsExpiredCommand() {
    CommandGuard guard(5, 1000);
    TEST_ASSERT_EQUAL(CommandVerdict::Expired, guard.check("cmd-1", kNow - 1, 1, kNow, true, 0));
}

void testDuplicateIsNotReplayed() {
    CommandGuard guard(5, 1000);
    TEST_ASSERT_EQUAL(CommandVerdict::Accept, guard.check("cmd-1", kNow + 10000, 1, kNow, true, 0));
    TEST_ASSERT_EQUAL(CommandVerdict::Duplicate, guard.check("cmd-1", kNow + 10000, 1, kNow, true, 3000));
}

void testExpiredCommandIsNotRemembered() {
    CommandGuard guard(5, 1000);
    guard.check("cmd-1", kNow - 1, 1, kNow, true, 0);
    TEST_ASSERT_EQUAL(CommandVerdict::Accept, guard.check("cmd-1", kNow + 10000, 2, kNow, true, 0));
}

void testWithoutClockUsesSequence() {
    CommandGuard guard(5, 1000);
    TEST_ASSERT_EQUAL(CommandVerdict::Accept, guard.check("a", 0, 5, 0, false, 0));
    TEST_ASSERT_EQUAL(CommandVerdict::StaleSequence, guard.check("b", 0, 5, 0, false, 0));
    TEST_ASSERT_EQUAL(CommandVerdict::StaleSequence, guard.check("c", 0, 3, 0, false, 0));
    TEST_ASSERT_EQUAL(CommandVerdict::Accept, guard.check("d", 0, 6, 0, false, 0));
}

void testRateLimit() {
    CommandGuard guard(3, 1000);
    char id[8];
    for (int i = 0; i < 3; i++) {
        snprintf(id, sizeof(id), "c%d", i);
        TEST_ASSERT_EQUAL(CommandVerdict::Accept, guard.check(id, kNow + 10000, i + 1, kNow, true, 100));
    }
    TEST_ASSERT_EQUAL(CommandVerdict::RateLimited, guard.check("c3", kNow + 10000, 4, kNow, true, 200));
    // Sau khi cửa sổ trôi qua thì được phép lại
    TEST_ASSERT_EQUAL(CommandVerdict::Accept, guard.check("c4", kNow + 10000, 5, kNow, true, 1200));
}

void testHistoryForgetsOldIds() {
    CommandGuard guard(16, 1);
    char id[8];
    for (int i = 0; i <= CommandGuard::kHistorySize; i++) {
        snprintf(id, sizeof(id), "c%d", i);
        guard.check(id, kNow + 10000, i + 1, kNow, true, i * 10);
    }
    // c0 đã bị đẩy ra khỏi lịch sử
    TEST_ASSERT_EQUAL(CommandVerdict::Accept, guard.check("c0", kNow + 10000, 100, kNow, true, 1000));
}

void testRateLimiterWrapsAround() {
    RateLimiter limiter(2, 100);
    TEST_ASSERT_TRUE(limiter.tryAcquire(UINT32_MAX - 10));
    TEST_ASSERT_TRUE(limiter.tryAcquire(UINT32_MAX - 5));
    TEST_ASSERT_FALSE(limiter.tryAcquire(20));  // millis() tràn số vẫn đúng
    TEST_ASSERT_TRUE(limiter.tryAcquire(200));
}

void testDuplicateReturnsOriginalResult() {
    CommandGuard guard(5, 1000);
    guard.check("ok-cmd", kNow + 10000, 1, kNow, true, 0);
    guard.setResult("ok-cmd", nullptr);
    guard.check("bad-cmd", kNow + 10000, 2, kNow, true, 0);
    guard.setResult("bad-cmd", "E_CMD_NOT_FOUND");

    const char* error = "sentinel";
    TEST_ASSERT_TRUE(guard.findResult("ok-cmd", error));
    TEST_ASSERT_NULL(error);
    TEST_ASSERT_TRUE(guard.findResult("bad-cmd", error));
    TEST_ASSERT_EQUAL_STRING("E_CMD_NOT_FOUND", error);
    TEST_ASSERT_FALSE(guard.findResult("unknown", error));
}

void testResultForgottenWithId() {
    CommandGuard guard(16, 1);
    guard.check("first", kNow + 10000, 1, kNow, true, 0);
    guard.setResult("first", nullptr);
    char id[8];
    for (int i = 0; i < CommandGuard::kHistorySize; i++) {
        snprintf(id, sizeof(id), "c%d", i);
        guard.check(id, kNow + 10000, i + 2, kNow, true, (i + 1) * 10);
    }
    const char* error = nullptr;
    TEST_ASSERT_FALSE(guard.findResult("first", error));
    TEST_ASSERT_FALSE(guard.findResult("c0", error));  // Đã nhớ id nhưng chưa có kết quả
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testAcceptsFreshCommand);
    RUN_TEST(testRejectsExpiredCommand);
    RUN_TEST(testDuplicateIsNotReplayed);
    RUN_TEST(testExpiredCommandIsNotRemembered);
    RUN_TEST(testWithoutClockUsesSequence);
    RUN_TEST(testRateLimit);
    RUN_TEST(testHistoryForgetsOldIds);
    RUN_TEST(testRateLimiterWrapsAround);
    RUN_TEST(testDuplicateReturnsOriginalResult);
    RUN_TEST(testResultForgottenWithId);
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
