#include <WifiQr.h>
#include <string.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void testBuildsStandardPayload() {
    char out[64];
    TEST_ASSERT_TRUE(buildWifiQrPayload(out, sizeof(out), "SmartHome-1A2B", "abcd2345xy"));
    TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:SmartHome-1A2B;P:abcd2345xy;;", out);
}

void testEscapesSpecialChars() {
    char out[64];
    TEST_ASSERT_TRUE(buildWifiQrPayload(out, sizeof(out), "a;b,c", "p:q\"r\\s"));
    TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:a\\;b\\,c;P:p\\:q\\\"r\\\\s;;", out);
}

void testFailsWhenBufferTooSmall() {
    char out[20];
    TEST_ASSERT_FALSE(buildWifiQrPayload(out, sizeof(out), "SmartHome-1A2B", "abcd2345xy"));
    TEST_ASSERT_EQUAL_STRING("", out);  // Không để lại chuỗi cắt dở
}

void testExactFitIncludingTerminator() {
    const char* expected = "WIFI:T:WPA;S:s;P:p;;";
    char out[21];  // strlen(expected) = 20, + '\0'
    TEST_ASSERT_TRUE(buildWifiQrPayload(out, sizeof(out), "s", "p"));
    TEST_ASSERT_EQUAL_STRING(expected, out);
    char tooSmall[20];
    TEST_ASSERT_FALSE(buildWifiQrPayload(tooSmall, sizeof(tooSmall), "s", "p"));
}

void testRejectsNullArguments() {
    char out[64];
    TEST_ASSERT_FALSE(buildWifiQrPayload(nullptr, sizeof(out), "s", "p"));
    TEST_ASSERT_FALSE(buildWifiQrPayload(out, sizeof(out), nullptr, "p"));
    TEST_ASSERT_FALSE(buildWifiQrPayload(out, sizeof(out), "s", nullptr));
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testBuildsStandardPayload);
    RUN_TEST(testEscapesSpecialChars);
    RUN_TEST(testFailsWhenBufferTooSmall);
    RUN_TEST(testExactFitIncludingTerminator);
    RUN_TEST(testRejectsNullArguments);
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
