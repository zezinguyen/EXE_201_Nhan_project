#include <BatteryFilter.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void testMedianOddAndEven() {
    uint16_t odd[] = {3900, 3700, 3810, 3820, 3815};
    TEST_ASSERT_EQUAL_UINT16(3815, medianMillivolts(odd, 5));
    uint16_t even[] = {3800, 3820, 3600, 3810};
    TEST_ASSERT_EQUAL_UINT16(3805, medianMillivolts(even, 4));
    TEST_ASSERT_EQUAL_UINT16(0, medianMillivolts(nullptr, 0));
}

void testMedianIgnoresSpikes() {
    // WiFi phát -> vài mẫu tụt sâu; trung vị không bị kéo theo như trung bình
    uint16_t samples[] = {3810, 3812, 3400, 3808, 3811, 3350, 3809, 3813, 3807};
    TEST_ASSERT_EQUAL_UINT16(3809, medianMillivolts(samples, 9));
}

void testFirstValueTakenAsIs() {
    BatteryFilter filter(3);
    TEST_ASSERT_EQUAL_UINT16(3810, filter.add(3810));
}

void testSmoothsAlternatingNoise() {
    // Đo nhảy 3810 <-> 3880 (47% <-> 60%): giá trị lọc chỉ dao động trong biên độ nhỏ
    BatteryFilter filter(3);
    filter.add(3845);
    uint16_t lo = 0xFFFF, hi = 0;
    for (int i = 0; i < 40; i++) {
        const uint16_t v = filter.add(i % 2 == 0 ? 3810 : 3880);
        if (i >= 20) {
            lo = v < lo ? v : lo;
            hi = v > hi ? v : hi;
        }
    }
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(10, hi - lo);
    TEST_ASSERT_UINT16_WITHIN(10, 3845, lo);
}

void testFollowsRealChangeAndReset() {
    BatteryFilter filter(3);
    filter.add(3800);
    for (int i = 0; i < 60; i++) {
        filter.add(3700);
    }
    TEST_ASSERT_UINT16_WITHIN(2, 3700, filter.value());
    filter.reset();
    TEST_ASSERT_EQUAL_UINT16(4150, filter.add(4150));
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testMedianOddAndEven);
    RUN_TEST(testMedianIgnoresSpikes);
    RUN_TEST(testFirstValueTakenAsIs);
    RUN_TEST(testSmoothsAlternatingNoise);
    RUN_TEST(testFollowsRealChangeAndReset);
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
