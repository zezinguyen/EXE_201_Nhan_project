#include <BatteryGauge.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

static BatteryGauge makeGauge() { return BatteryGauge({20, 10, 5}, 2); }

void testPercentAtCurveEnds() {
    TEST_ASSERT_EQUAL_UINT8(100, BatteryGauge::percentFromMillivolts(4300));
    TEST_ASSERT_EQUAL_UINT8(100, BatteryGauge::percentFromMillivolts(4200));
    TEST_ASSERT_EQUAL_UINT8(0, BatteryGauge::percentFromMillivolts(3300));
    TEST_ASSERT_EQUAL_UINT8(0, BatteryGauge::percentFromMillivolts(2800));
}

void testPercentInterpolates() {
    TEST_ASSERT_EQUAL_UINT8(95, BatteryGauge::percentFromMillivolts(4150));
    TEST_ASSERT_EQUAL_UINT8(20, BatteryGauge::percentFromMillivolts(3700));
}

void testPercentIsMonotonic() {
    uint8_t previous = 0;
    for (uint16_t mv = 3000; mv <= 4300; mv += 5) {
        const uint8_t percent = BatteryGauge::percentFromMillivolts(mv);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT8(previous, percent);
        previous = percent;
    }
}

void testLevelThresholds() {
    BatteryGauge gauge = makeGauge();
    TEST_ASSERT_EQUAL(BatteryLevel::Normal, gauge.update(50));
    TEST_ASSERT_EQUAL(BatteryLevel::Low, gauge.update(20));
    TEST_ASSERT_EQUAL(BatteryLevel::Critical, gauge.update(10));
    TEST_ASSERT_EQUAL(BatteryLevel::Empty, gauge.update(5));
}

void testFirstReadingSetsLevelDirectly() {
    BatteryGauge gauge = makeGauge();
    TEST_ASSERT_EQUAL(BatteryLevel::Empty, gauge.update(3));
    BatteryGauge other = makeGauge();
    TEST_ASSERT_EQUAL(BatteryLevel::Normal, other.update(90));
}

void testHysteresisOnRecovery() {
    BatteryGauge gauge = makeGauge();
    gauge.update(19);
    TEST_ASSERT_EQUAL(BatteryLevel::Low, gauge.level());
    TEST_ASSERT_EQUAL(BatteryLevel::Low, gauge.update(21));  // chưa vượt 20 + 2
    TEST_ASSERT_EQUAL(BatteryLevel::Low, gauge.update(22));
    TEST_ASSERT_EQUAL(BatteryLevel::Normal, gauge.update(23));
}

void testChargingJumpsMultipleLevels() {
    BatteryGauge gauge = makeGauge();
    gauge.update(4);
    TEST_ASSERT_EQUAL(BatteryLevel::Normal, gauge.update(80));
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testPercentAtCurveEnds);
    RUN_TEST(testPercentInterpolates);
    RUN_TEST(testPercentIsMonotonic);
    RUN_TEST(testLevelThresholds);
    RUN_TEST(testFirstReadingSetsLevelDirectly);
    RUN_TEST(testHysteresisOnRecovery);
    RUN_TEST(testChargingJumpsMultipleLevels);
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
