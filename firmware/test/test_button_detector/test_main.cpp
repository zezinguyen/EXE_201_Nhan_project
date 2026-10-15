#include <ButtonDetector.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

static const uint32_t kLevels[] = {5000, 10000, 20000};

static ButtonDetector makeButton() { return ButtonDetector(30, kLevels, 3); }

// Giữ mức raw từ fromMs đến toMs (bước 5ms), trả về sự kiện khác None đầu tiên.
static ButtonEvent hold(ButtonDetector& button, bool pressed, uint32_t fromMs, uint32_t toMs) {
    ButtonEvent result = {PressType::None, 0};
    for (uint32_t t = fromMs; t <= toMs; t += 5) {
        const ButtonEvent event = button.update(pressed, t);
        if (event.type != PressType::None && result.type == PressType::None) {
            result = event;
        }
    }
    return result;
}

static void assertEvent(PressType type, uint8_t level, ButtonEvent event) {
    TEST_ASSERT_EQUAL(type, event.type);
    TEST_ASSERT_EQUAL_UINT8(level, event.holdLevel);
}

void testShortPress() {
    ButtonDetector button = makeButton();
    assertEvent(PressType::None, 0, hold(button, true, 0, 200));
    TEST_ASSERT_TRUE(button.isPressed());
    assertEvent(PressType::Short, 0, hold(button, false, 205, 300));
    TEST_ASSERT_FALSE(button.isPressed());
}

void testBounceIsIgnored() {
    ButtonDetector button = makeButton();
    for (uint32_t t = 0; t < 100; t += 10) {
        TEST_ASSERT_EQUAL(PressType::None, button.update((t / 10) % 2 == 0, t).type);
    }
    TEST_ASSERT_FALSE(button.isPressed());
}

void testEachHoldLevelFiresOnRelease() {
    const uint32_t holdUntil[] = {6000, 12000, 25000};
    for (uint8_t i = 0; i < 3; i++) {
        ButtonDetector button = makeButton();
        assertEvent(PressType::None, 0, hold(button, true, 0, holdUntil[i]));
        assertEvent(PressType::Hold, i + 1, hold(button, false, holdUntil[i] + 5, holdUntil[i] + 100));
    }
}

void testCurrentHoldLevelWhileHeld() {
    ButtonDetector button = makeButton();
    TEST_ASSERT_EQUAL_UINT8(0, button.currentHoldLevel(0));
    hold(button, true, 0, 4000);
    TEST_ASSERT_EQUAL_UINT8(0, button.currentHoldLevel(4000));
    hold(button, true, 4005, 10000);
    TEST_ASSERT_EQUAL_UINT8(2, button.currentHoldLevel(10000));
    hold(button, false, 10005, 10100);
    TEST_ASSERT_EQUAL_UINT8(0, button.currentHoldLevel(10100));
}

void testNoLevelsGivesOnlyShortPress() {
    ButtonDetector button(30, nullptr, 0);
    hold(button, true, 0, 30000);
    assertEvent(PressType::Short, 0, hold(button, false, 30005, 30100));
}

void testHeldMs() {
    ButtonDetector button = makeButton();
    TEST_ASSERT_EQUAL_UINT32(0, button.heldMs(0));
    hold(button, true, 100, 1100);
    TEST_ASSERT_EQUAL_UINT32(1000, button.heldMs(1100));
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testShortPress);
    RUN_TEST(testBounceIsIgnored);
    RUN_TEST(testEachHoldLevelFiresOnRelease);
    RUN_TEST(testCurrentHoldLevelWhileHeld);
    RUN_TEST(testNoLevelsGivesOnlyShortPress);
    RUN_TEST(testHeldMs);
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
