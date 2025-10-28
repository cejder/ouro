#include "memory.hpp"
#include "test.hpp"
#include "unit.hpp"

#include <unity.h>

void static test_unit_to_pretty_time_i() {
    S64 const value = 1000000;
    auto *buffer = mmta(C8 *, sizeof(C8) * PRETTY_BUFFER_SIZE);
    if (!buffer) { TEST_FAIL_MESSAGE("Failed to allocate buffer"); }
    unit_to_pretty_time_i(value, buffer, PRETTY_BUFFER_SIZE, UNIT_TIME_DAYS);
    TEST_ASSERT_EQUAL_STRING("1ms", buffer);
}

void static test_unit_to_pretty_time_u() {
    U64 const value = 1000000;
    auto *buffer = mmta(C8 *, sizeof(C8) * PRETTY_BUFFER_SIZE);
    if (!buffer) { TEST_FAIL_MESSAGE("Failed to allocate buffer"); }
    unit_to_pretty_time_u(value, buffer, PRETTY_BUFFER_SIZE, UNIT_TIME_DAYS);
    TEST_ASSERT_EQUAL_STRING("1ms", buffer);
}

void static test_unit_to_pretty_time_f() {
    F64 const value = 1000000;
    auto *buffer = mmta(C8 *, sizeof(C8) * PRETTY_BUFFER_SIZE);
    if (!buffer) { TEST_FAIL_MESSAGE("Failed to allocate buffer"); }
    unit_to_pretty_time_f(value, buffer, PRETTY_BUFFER_SIZE, UNIT_TIME_DAYS);
    TEST_ASSERT_EQUAL_STRING("1.00ms", buffer);
}

void static test_unit_to_pretty_prefix_i() {
    S64 const value = 1000000;
    auto *buffer = mmta(C8 *, sizeof(C8) * PRETTY_BUFFER_SIZE);
    if (!buffer) { TEST_FAIL_MESSAGE("Failed to allocate buffer"); }
    unit_to_pretty_prefix_i("Hz", value, buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    TEST_ASSERT_EQUAL_STRING("1.00MHz", buffer);
}

void static test_unit_to_pretty_prefix_u() {
    U64 const value = 1000000;
    auto *buffer = mmta(C8 *, sizeof(C8) * PRETTY_BUFFER_SIZE);
    if (!buffer) { TEST_FAIL_MESSAGE("Failed to allocate buffer"); }
    unit_to_pretty_prefix_u("Hz", value, buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    TEST_ASSERT_EQUAL_STRING("1.00MHz", buffer);
}

void static test_unit_to_pretty_prefix_f() {
    F64 const value = 1000000;
    auto *buffer = mmta(C8 *, sizeof(C8) * PRETTY_BUFFER_SIZE);
    if (!buffer) { TEST_FAIL_MESSAGE("Failed to allocate buffer"); }
    unit_to_pretty_prefix_f("Hz", value, buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    TEST_ASSERT_EQUAL_STRING("1.00MHz", buffer);
}

void static test_unit_to_pretty_prefix_binary_i() {
    S64 const value = 1024;
    auto *buffer = mmta(C8 *, sizeof(C8) * PRETTY_BUFFER_SIZE);
    if (!buffer) { TEST_FAIL_MESSAGE("Failed to allocate buffer"); }
    unit_to_pretty_prefix_binary_i("B", value, buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_BINARY_GIBI);
    TEST_ASSERT_EQUAL_STRING("1.00KiB", buffer);
}

void static test_unit_to_pretty_prefix_binary_u() {
    U64 const value = 1024;
    auto *buffer = mmta(C8 *, sizeof(C8) * PRETTY_BUFFER_SIZE);
    if (!buffer) { TEST_FAIL_MESSAGE("Failed to allocate buffer"); }
    unit_to_pretty_prefix_binary_u("B", value, buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_BINARY_GIBI);
    TEST_ASSERT_EQUAL_STRING("1.00KiB", buffer);
}

void static test_unit_to_pretty_prefix_binary_f() {
    F64 const value = 1024;
    auto *buffer = mmta(C8 *, sizeof(C8) * PRETTY_BUFFER_SIZE);
    if (!buffer) { TEST_FAIL_MESSAGE("Failed to allocate buffer"); }
    unit_to_pretty_prefix_binary_f("B", value, buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_BINARY_GIBI);
    TEST_ASSERT_EQUAL_STRING("1.00KiB", buffer);
}

void test_unit() {
    RUN_TEST(test_unit_to_pretty_time_i);
    RUN_TEST(test_unit_to_pretty_time_u);
    RUN_TEST(test_unit_to_pretty_time_f);
    RUN_TEST(test_unit_to_pretty_prefix_i);
    RUN_TEST(test_unit_to_pretty_prefix_u);
    RUN_TEST(test_unit_to_pretty_prefix_f);
    RUN_TEST(test_unit_to_pretty_prefix_binary_i);
    RUN_TEST(test_unit_to_pretty_prefix_binary_u);
    RUN_TEST(test_unit_to_pretty_prefix_binary_f);
}
