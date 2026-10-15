#include <Password.h>
#include <string.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

static uint32_t counter = 0;
static uint32_t countingRandom() { return counter++; }
static uint32_t lcgState = 12345;
static uint32_t lcgRandom() {
    lcgState = lcgState * 1664525u + 1013904223u;
    return lcgState;
}

void testGeneratesRequestedLength() {
    char out[16];
    TEST_ASSERT_TRUE(generatePassword(out, sizeof(out), 10, lcgRandom));
    TEST_ASSERT_EQUAL_size_t(10, strlen(out));
}

void testUsesOnlyUnambiguousChars() {
    char out[64];
    TEST_ASSERT_TRUE(generatePassword(out, sizeof(out), 63, lcgRandom));
    for (const char* c = out; *c != '\0'; c++) {
        TEST_ASSERT_NOT_NULL(strchr(kPasswordAlphabet, *c));
        TEST_ASSERT_NULL(strchr("0o1li", *c));
    }
}

void testMapsRandomToAlphabet() {
    counter = 0;
    char out[9];
    TEST_ASSERT_TRUE(generatePassword(out, sizeof(out), 8, countingRandom));
    TEST_ASSERT_EQUAL_STRING("abcdefgh", out);
}

void testRejectsShortPassword() {
    char out[16];
    TEST_ASSERT_FALSE(generatePassword(out, sizeof(out), 7, lcgRandom));
}

void testRejectsSmallBuffer() {
    char out[10];
    TEST_ASSERT_FALSE(generatePassword(out, sizeof(out), 10, lcgRandom));
    TEST_ASSERT_FALSE(generatePassword(nullptr, 0, 10, lcgRandom));
    TEST_ASSERT_FALSE(generatePassword(out, sizeof(out), 8, nullptr));
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testGeneratesRequestedLength);
    RUN_TEST(testUsesOnlyUnambiguousChars);
    RUN_TEST(testMapsRandomToAlphabet);
    RUN_TEST(testRejectsShortPassword);
    RUN_TEST(testRejectsSmallBuffer);
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
