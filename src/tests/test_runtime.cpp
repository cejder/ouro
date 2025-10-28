#include "test.hpp"

#include <unity.h>

void static test_runtime_requirements() {
    TEST_ASSERT_EQUAL_INT8(1, sizeof(S8));
    TEST_ASSERT_EQUAL_INT8(1, sizeof(U8));
    TEST_ASSERT_EQUAL_INT8(2, sizeof(S16));
    TEST_ASSERT_EQUAL_INT8(2, sizeof(U16));
    TEST_ASSERT_EQUAL_INT8(4, sizeof(S32));
    TEST_ASSERT_EQUAL_INT8(4, sizeof(U32));
    TEST_ASSERT_EQUAL_INT8(8, sizeof(S64));
    TEST_ASSERT_EQUAL_INT8(8, sizeof(U64));
    TEST_ASSERT_EQUAL_INT8(4, sizeof(F32));
    TEST_ASSERT_EQUAL_INT8(8, sizeof(F64));
    TEST_ASSERT_EQUAL_INT8(8, sizeof(SZ));
}

void test_runtime() {
    RUN_TEST(test_runtime_requirements);
}
