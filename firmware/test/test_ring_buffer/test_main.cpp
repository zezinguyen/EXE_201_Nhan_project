#include <RingBuffer.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void testPushPopFifo() {
    RingBuffer<int, 3> buffer;
    TEST_ASSERT_TRUE(buffer.empty());
    buffer.push(1);
    buffer.push(2);
    int value = 0;
    TEST_ASSERT_TRUE(buffer.pop(value));
    TEST_ASSERT_EQUAL_INT(1, value);
    TEST_ASSERT_TRUE(buffer.pop(value));
    TEST_ASSERT_EQUAL_INT(2, value);
    TEST_ASSERT_FALSE(buffer.pop(value));
}

void testOverwritesOldestWhenFull() {
    RingBuffer<int, 3> buffer;
    TEST_ASSERT_TRUE(buffer.push(1));
    TEST_ASSERT_TRUE(buffer.push(2));
    TEST_ASSERT_TRUE(buffer.push(3));
    TEST_ASSERT_TRUE(buffer.full());
    TEST_ASSERT_FALSE(buffer.push(4));
    TEST_ASSERT_EQUAL_size_t(3, buffer.size());
    TEST_ASSERT_EQUAL_INT(2, buffer.at(0));
    TEST_ASSERT_EQUAL_INT(4, buffer.at(2));
}

void testPeekDoesNotRemove() {
    RingBuffer<int, 2> buffer;
    int value = 0;
    TEST_ASSERT_FALSE(buffer.peek(value));
    buffer.push(7);
    TEST_ASSERT_TRUE(buffer.peek(value));
    TEST_ASSERT_EQUAL_INT(7, value);
    TEST_ASSERT_EQUAL_size_t(1, buffer.size());
}

void testClear() {
    RingBuffer<int, 2> buffer;
    buffer.push(1);
    buffer.clear();
    TEST_ASSERT_TRUE(buffer.empty());
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testPushPopFifo);
    RUN_TEST(testOverwritesOldestWhenFull);
    RUN_TEST(testPeekDoesNotRemove);
    RUN_TEST(testClear);
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
