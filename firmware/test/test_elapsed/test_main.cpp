#include <Elapsed.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void testNormalElapsed() {
    TEST_ASSERT_EQUAL_UINT32(0, elapsedMs(1000, 1000));
    TEST_ASSERT_EQUAL_UINT32(500, elapsedMs(1500, 1000));
}

void testStartAfterNowIsZero() {
    // nowMs lấy đầu vòng lặp, startMs = millis() giữa vòng -> không được coi là hết giờ
    TEST_ASSERT_EQUAL_UINT32(0, elapsedMs(39423, 39933));
}

void testMillisOverflow() {
    TEST_ASSERT_EQUAL_UINT32(20, elapsedMs(10, 0xFFFFFFF6u));
    TEST_ASSERT_EQUAL_UINT32(0, elapsedMs(0xFFFFFFF6u, 10));
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testNormalElapsed);
    RUN_TEST(testStartAfterNowIsZero);
    RUN_TEST(testMillisOverflow);
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
