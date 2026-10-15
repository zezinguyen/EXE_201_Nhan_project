#include <Backoff.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void testDoublesUntilMax() {
    Backoff backoff(2000, 60000, 0);
    const uint32_t expected[] = {2000, 4000, 8000, 16000, 32000, 60000, 60000};
    for (uint32_t value : expected) {
        TEST_ASSERT_EQUAL_UINT32(value, backoff.nextDelayMs(0));
    }
    TEST_ASSERT_EQUAL_UINT16(7, backoff.attempts());
}

void testResetReturnsToBase() {
    Backoff backoff(2000, 60000, 0);
    backoff.nextDelayMs(0);
    backoff.nextDelayMs(0);
    backoff.reset();
    TEST_ASSERT_EQUAL_UINT32(2000, backoff.nextDelayMs(0));
    TEST_ASSERT_EQUAL_UINT16(1, backoff.attempts());
}

void testJitterStaysInRange() {
    Backoff backoff(1000, 1000, 20);
    for (uint32_t random = 0; random < 1000; random += 37) {
        const uint32_t delay = backoff.nextDelayMs(random);
        TEST_ASSERT_UINT32_WITHIN(200, 1000, delay);
    }
}

void testJitterUsesRandomValue() {
    Backoff backoff(1000, 1000, 20);
    TEST_ASSERT_EQUAL_UINT32(800, backoff.nextDelayMs(0));
    TEST_ASSERT_EQUAL_UINT32(1200, backoff.nextDelayMs(400));
}

void testMaxBelowBaseIsClamped() {
    Backoff backoff(5000, 1000, 0);
    TEST_ASSERT_EQUAL_UINT32(5000, backoff.nextDelayMs(0));
    TEST_ASSERT_EQUAL_UINT32(5000, backoff.nextDelayMs(0));
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testDoublesUntilMax);
    RUN_TEST(testResetReturnsToBase);
    RUN_TEST(testJitterStaysInRange);
    RUN_TEST(testJitterUsesRandomValue);
    RUN_TEST(testMaxBelowBaseIsClamped);
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
