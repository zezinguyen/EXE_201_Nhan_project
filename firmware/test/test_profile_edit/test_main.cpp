#include <ProfileEdit.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

namespace {
JsonDocument sampleProfile() {
    JsonDocument doc;
    deserializeJson(doc, R"({"profileVersion":5,"devices":{
        "tv":{"type":"tv","commands":{"POWER":{"protocol":"NEC","code":"0x20DF10EF","bits":32}}},
        "ac":{"type":"ac","irac_protocol":"MIDEA"}}})");
    return doc;
}

JsonDocument rawCode() {
    JsonDocument ir;
    deserializeJson(ir, R"({"raw":[9000,4500,560],"freq":38})");
    return ir;
}
}  // namespace

void testNameValidation() {
    TEST_ASSERT_TRUE(isValidProfileName("POWER", 32));
    TEST_ASSERT_TRUE(isValidProfileName("Bật 25 độ", 32));  // UTF-8
    TEST_ASSERT_FALSE(isValidProfileName("", 32));
    TEST_ASSERT_FALSE(isValidProfileName(nullptr, 32));
    TEST_ASSERT_FALSE(isValidProfileName("a\nb", 32));
    TEST_ASSERT_TRUE(isValidProfileName("abcd", 4));
    TEST_ASSERT_FALSE(isValidProfileName("abcde", 4));
}

void testEnsureAcCreatesProfileFromEmpty() {
    JsonDocument doc;
    TEST_ASSERT_TRUE(profileEnsureAc(doc, "ac_funiki", "Funiki", "MIDEA"));
    TEST_ASSERT_EQUAL_UINT32(1, doc["profileVersion"].as<uint32_t>());
    TEST_ASSERT_EQUAL_STRING("MIDEA", doc["devices"]["ac_funiki"]["irac_protocol"]);
}

void testEnsureAcKeepsExistingDevice() {
    JsonDocument doc = sampleProfile();
    TEST_ASSERT_FALSE(profileEnsureAc(doc, "ac", "x", "GREE"));
    TEST_ASSERT_EQUAL_STRING("MIDEA", doc["devices"]["ac"]["irac_protocol"]);
    TEST_ASSERT_EQUAL_UINT32(5, doc["profileVersion"].as<uint32_t>());
}

void testSetAcProtocol() {
    JsonDocument doc = sampleProfile();
    TEST_ASSERT_TRUE(profileSetAcProtocol(doc, "ac", "GREE"));
    TEST_ASSERT_EQUAL_STRING("GREE", doc["devices"]["ac"]["irac_protocol"]);
    TEST_ASSERT_EQUAL_UINT32(6, doc["profileVersion"].as<uint32_t>());
    TEST_ASSERT_FALSE(profileSetAcProtocol(doc, "ac", "GREE"));  // không đổi
    TEST_ASSERT_FALSE(profileSetAcProtocol(doc, "tv", "GREE"));  // không phải điều hòa
    TEST_ASSERT_FALSE(profileSetAcProtocol(doc, "none", "GREE"));
}

void testPutCommandOnExistingAndNewDevice() {
    JsonDocument doc = sampleProfile();
    JsonDocument ir = rawCode();
    TEST_ASSERT_TRUE(profilePutCommand(doc, "tv", nullptr, "MENU", ir.as<JsonObjectConst>()));
    TEST_ASSERT_EQUAL(3, doc["devices"]["tv"]["commands"]["MENU"]["raw"].size());
    TEST_ASSERT_EQUAL_STRING("NEC", doc["devices"]["tv"]["commands"]["POWER"]["protocol"]);

    TEST_ASSERT_TRUE(profilePutCommand(doc, "remote_hoc", "Phím đã học", "Quạt", ir.as<JsonObjectConst>()));
    TEST_ASSERT_EQUAL_STRING("custom", doc["devices"]["remote_hoc"]["type"]);
    TEST_ASSERT_EQUAL_STRING("Phím đã học", doc["devices"]["remote_hoc"]["name"]);
    TEST_ASSERT_EQUAL_UINT32(7, doc["profileVersion"].as<uint32_t>());
}

void testPutCommandRejectsAcAndBadCode() {
    JsonDocument doc = sampleProfile();
    JsonDocument ir = rawCode();
    TEST_ASSERT_FALSE(profilePutCommand(doc, "ac", nullptr, "X", ir.as<JsonObjectConst>()));

    JsonDocument bad;
    deserializeJson(bad, R"({"protocol":"NEC","code":"0x1"})");  // thiếu bits
    TEST_ASSERT_FALSE(profilePutCommand(doc, "tv", nullptr, "X", bad.as<JsonObjectConst>()));
    deserializeJson(bad, R"({"raw":[]})");
    TEST_ASSERT_FALSE(profilePutCommand(doc, "tv", nullptr, "X", bad.as<JsonObjectConst>()));
    TEST_ASSERT_EQUAL_UINT32(5, doc["profileVersion"].as<uint32_t>());
}

void testRemoveCommand() {
    JsonDocument doc = sampleProfile();
    TEST_ASSERT_TRUE(profileRemoveCommand(doc, "tv", "POWER"));
    TEST_ASSERT_TRUE(doc["devices"]["tv"]["commands"]["POWER"].isNull());
    TEST_ASSERT_FALSE(profileRemoveCommand(doc, "tv", "POWER"));
    TEST_ASSERT_FALSE(profileRemoveCommand(doc, "ac", "POWER"));
    TEST_ASSERT_EQUAL_UINT32(6, doc["profileVersion"].as<uint32_t>());
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testNameValidation);
    RUN_TEST(testEnsureAcCreatesProfileFromEmpty);
    RUN_TEST(testEnsureAcKeepsExistingDevice);
    RUN_TEST(testSetAcProtocol);
    RUN_TEST(testPutCommandOnExistingAndNewDevice);
    RUN_TEST(testPutCommandRejectsAcAndBadCode);
    RUN_TEST(testRemoveCommand);
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
