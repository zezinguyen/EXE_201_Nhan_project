#include <IrCompare.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void testIdenticalCapturesMatch() {
    const uint16_t a[] = {9000, 4500, 560, 560, 560, 1690};
    TEST_ASSERT_TRUE(rawCapturesMatch(a, 6, a, 6, 25, 150));
}

void testSmallNoiseMatches() {
    const uint16_t a[] = {9000, 4500, 560, 560, 560, 1690};
    const uint16_t b[] = {8900, 4600, 600, 520, 640, 1650};
    TEST_ASSERT_TRUE(rawCapturesMatch(a, 6, b, 6, 25, 150));
}

void testDifferentLengthFails() {
    const uint16_t a[] = {9000, 4500, 560, 560};
    TEST_ASSERT_FALSE(rawCapturesMatch(a, 4, a, 3, 25, 150));
}

void testLargeDifferenceFails() {
    const uint16_t a[] = {9000, 4500, 560, 560};
    const uint16_t b[] = {9000, 4500, 560, 1690};
    TEST_ASSERT_FALSE(rawCapturesMatch(a, 4, b, 4, 25, 150));
}

void testMinToleranceForShortPulses() {
    const uint16_t a[] = {100};
    const uint16_t b[] = {220};
    TEST_ASSERT_TRUE(rawCapturesMatch(a, 1, b, 1, 10, 150));
    TEST_ASSERT_FALSE(rawCapturesMatch(a, 1, b, 1, 10, 100));
}

void testEmptyOrNullFails() {
    const uint16_t a[] = {1};
    TEST_ASSERT_FALSE(rawCapturesMatch(nullptr, 0, nullptr, 0, 25, 150));
    TEST_ASSERT_FALSE(rawCapturesMatch(a, 0, a, 0, 25, 150));
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testIdenticalCapturesMatch);
    RUN_TEST(testSmallNoiseMatches);
    RUN_TEST(testDifferentLengthFails);
    RUN_TEST(testLargeDifferenceFails);
    RUN_TEST(testMinToleranceForShortPulses);
    RUN_TEST(testEmptyOrNullFails);
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
