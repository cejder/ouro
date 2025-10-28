#include "array.hpp"
#include "log.hpp"
#include "memory.hpp"
#include "test.hpp"
#include "time.hpp"
#include "unit.hpp"

#include <unity.h>

ARRAY_DECLARE(TestU64Array, U64);

void static test_array_init() {
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 0);
    TEST_ASSERT_EQUAL_INT(array1.capacity, 0);
    TEST_ASSERT_EQUAL_INT(array1.count, 0);
    TEST_ASSERT_EQUAL_INT(array1.mem_type, MEMORY_TYPE_TARENA);
    TEST_ASSERT_NULL(array1.data);

    TestU64Array array2;
    array_init(MEMORY_TYPE_TARENA, &array2, 100);
    TEST_ASSERT_EQUAL_INT(array2.capacity, 100);
    TEST_ASSERT_EQUAL_INT(array2.count, 0);
    TEST_ASSERT_EQUAL_INT(array2.mem_type, MEMORY_TYPE_TARENA);
    TEST_ASSERT_NOT_NULL(array2.data);
}

void static test_array_push() {
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 0);
    array_push(&array1, 10);
    TEST_ASSERT_EQUAL_INT(array1.capacity, 8);
    TEST_ASSERT_EQUAL_INT(array1.count, 1);
    TEST_ASSERT_NOT_NULL(array1.data);
    array_push(&array1, 20);
    TEST_ASSERT_EQUAL_INT(array1.capacity, 8);
    TEST_ASSERT_EQUAL_INT(array1.count, 2);
    TEST_ASSERT_NOT_NULL(array1.data);
    TEST_ASSERT_TRUE(array1.data[0] == 10);
    TEST_ASSERT_TRUE(array1.data[1] == 20);

    TestU64Array array2;
    array_init(MEMORY_TYPE_TARENA, &array2, 100);
    array_push(&array2, 10);
    TEST_ASSERT_EQUAL_INT(array2.capacity, 100);
    TEST_ASSERT_EQUAL_INT(array2.count, 1);
    TEST_ASSERT_NOT_NULL(array2.data);
    array_push(&array2, 20);
    TEST_ASSERT_EQUAL_INT(array2.capacity, 100);
    TEST_ASSERT_EQUAL_INT(array2.count, 2);
    TEST_ASSERT_NOT_NULL(array2.data);
    TEST_ASSERT_TRUE(array2.data[0] == 10);
    TEST_ASSERT_TRUE(array2.data[1] == 20);
}

void static test_array_push_multiple() {
    U64 const arr[] = {10, 20};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 0);
    array_push_multiple(&array1, arr, 2);
    TEST_ASSERT_EQUAL_INT(array1.count, 2);
    TEST_ASSERT_TRUE(array1.data[0] == 10);
    TEST_ASSERT_TRUE(array1.data[1] == 20);
}

void static test_array_pop() {
    U64 const arr[] = {10, 20};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 0);
    array_push_multiple(&array1, arr, 2);
    U64 const popped = array_pop(&array1);
    TEST_ASSERT_TRUE(popped == 20);
    TEST_ASSERT_EQUAL_INT(array1.count, 1);
}

void static test_array_get() {
    U64 const arr[] = {10, 20, 30};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 0);
    array_push_multiple(&array1, arr, 3);
    U64 got = array_get(&array1, 0);
    TEST_ASSERT_TRUE(got == 10);
    got = array_get(&array1, 1);
    TEST_ASSERT_TRUE(got == 20);
    got = array_get(&array1, 2);
    TEST_ASSERT_TRUE(got == 30);
}

void static test_array_set() {
    U64 const arr[] = {0, 0, 0};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 0);
    array_push_multiple(&array1, arr, 3);
    array_set(&array1, 0, 10);
    array_set(&array1, 1, 20);
    array_set(&array1, 2, 30);
    U64 got = array_get(&array1, 0);
    TEST_ASSERT_TRUE(got == 10);
    got = array_get(&array1, 1);
    TEST_ASSERT_TRUE(got == 20);
    got = array_get(&array1, 2);
    TEST_ASSERT_TRUE(got == 30);
}

void static test_array_clear() {
    U64 const arr[] = {10, 20};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 0);
    array_push_multiple(&array1, arr, 2);
    array_clear(&array1);
    TEST_ASSERT_EQUAL_INT(array1.count, 0);
}

void static test_array_last() {
    U64 const arr[] = {10, 20, 30};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 0);
    array_push_multiple(&array1, arr, 3);
    U64 const got = array_last(&array1);
    TEST_ASSERT_TRUE(got == 30);
}

void static test_array_first() {
    U64 const arr[] = {10, 20, 30};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 0);
    array_push_multiple(&array1, arr, 3);
    U64 const got = array_first(&array1);
    TEST_ASSERT_TRUE(got == 10);
}

void static test_array_empty() {
    U64 const arr[] = {10, 20, 30};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 0);
    array_push_multiple(&array1, arr, 3);
    BOOL empty = array_empty(&array1);
    TEST_ASSERT_FALSE(empty);
    array_clear(&array1);
    empty = array_empty(&array1);
    TEST_ASSERT_TRUE(empty);
    array_push(&array1, 10);
    empty = array_empty(&array1);
    TEST_ASSERT_FALSE(empty);
}

void static test_array_full() {
    U64 const arr[] = {10, 20, 30};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 3);
    array_push_multiple(&array1, arr, 3);
    BOOL full = array_full(&array1);
    TEST_ASSERT_TRUE(full);
    array_clear(&array1);
    full = array_full(&array1);
    TEST_ASSERT_FALSE(full);
    array_push(&array1, 10);
    full = array_full(&array1);
    TEST_ASSERT_FALSE(full);
}

void static test_array_reserve() {
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 0);
    TEST_ASSERT_EQUAL_INT(array1.capacity, 0);
    array_reserve(&array1, 256);
    TEST_ASSERT_EQUAL_INT(array1.capacity, 256);
}

void static test_array_each() {
    U64 const arr[] = {10, 20, 30};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 3);
    array_push_multiple(&array1, arr, 3);
    array_each(i, &array1) {
        TEST_ASSERT_EQUAL_INT(array1.data[i], (i + 1) * 10);
    };
}

void static test_array_each_ptr() {
    U64 const arr[] = {10, 20, 30};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 3);
    array_push_multiple(&array1, arr, 3);
    array_each_ptr(ptr, &array1) {
        *ptr += 10;
    };
    array_each(i, &array1) {
        TEST_ASSERT_EQUAL_INT(array1.data[i], ((i + 1) * 10) + 10);
    };
}

void static test_array_each_reverse() {
    U64 const arr[] = {10, 20, 30};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 3);
    array_push_multiple(&array1, arr, 3);
    array_each_ptr(ptr, &array1) {
        *ptr += 10;
    };
    array_each_reverse(i, &array1) {
        TEST_ASSERT_EQUAL_INT(array1.data[i], arr[i] + 10);
    };
}

void static test_array_find() {
    U64 const arr[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 00};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 10);
    array_push_multiple(&array1, arr, 10);
    SZ result = 0;
    array_find(&array1, 00, result);
    TEST_ASSERT_EQUAL_INT(result, 9);
    array_find(&array1, 50, result);
    TEST_ASSERT_EQUAL_INT(result, 4);
}

void static test_array_remove_at() {
    U64 const arr[] = {10, 20, 30, 40, 50};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 5);
    array_push_multiple(&array1, arr, 5);
    array_remove_at(&array1, 1);
    TEST_ASSERT_EQUAL_INT(array1.data[0], 10);
    TEST_ASSERT_EQUAL_INT(array1.data[1], 30);
    TEST_ASSERT_EQUAL_INT(array1.data[2], 40);
    TEST_ASSERT_EQUAL_INT(array1.data[3], 50);
    TEST_ASSERT_EQUAL_INT(array1.count, 4);
    array_remove_at(&array1, 3);
    TEST_ASSERT_EQUAL_INT(array1.data[0], 10);
    TEST_ASSERT_EQUAL_INT(array1.data[1], 30);
    TEST_ASSERT_EQUAL_INT(array1.data[2], 40);
    TEST_ASSERT_EQUAL_INT(array1.count, 3);
}

void static test_array_remove_value() {
    U64 const arr[] = {10, 20, 30, 40, 50};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 5);
    array_push_multiple(&array1, arr, 5);
    array_remove_value(&array1, 20);
    TEST_ASSERT_EQUAL_INT(array1.data[0], 10);
    TEST_ASSERT_EQUAL_INT(array1.data[1], 30);
    TEST_ASSERT_EQUAL_INT(array1.data[2], 40);
    TEST_ASSERT_EQUAL_INT(array1.data[3], 50);
    TEST_ASSERT_EQUAL_INT(array1.count, 4);
    array_remove_value(&array1, 50);
    TEST_ASSERT_EQUAL_INT(array1.data[0], 10);
    TEST_ASSERT_EQUAL_INT(array1.data[1], 30);
    TEST_ASSERT_EQUAL_INT(array1.data[2], 40);
    TEST_ASSERT_EQUAL_INT(array1.count, 3);
}

void static test_array_last_ptr() {
    U64 const arr[] = {10, 20, 30};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 0);
    array_push_multiple(&array1, arr, 3);
    U64 *last_ptr = array_last_ptr(&array1);
    TEST_ASSERT_NOT_NULL(last_ptr);
    TEST_ASSERT_EQUAL_INT(*last_ptr, 30);
    *last_ptr = 99;
    TEST_ASSERT_EQUAL_INT(array1.data[2], 99);
    
    TestU64Array empty_array;
    array_init(MEMORY_TYPE_TARENA, &empty_array, 0);
    U64 *empty_last_ptr = array_last_ptr(&empty_array);
    TEST_ASSERT_NULL(empty_last_ptr);
}

void static test_array_first_ptr() {
    U64 const arr[] = {10, 20, 30};
    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 0);
    array_push_multiple(&array1, arr, 3);
    U64 *first_ptr = array_first_ptr(&array1);
    TEST_ASSERT_NOT_NULL(first_ptr);
    TEST_ASSERT_EQUAL_INT(*first_ptr, 10);
    *first_ptr = 99;
    TEST_ASSERT_EQUAL_INT(array1.data[0], 99);
    
    TestU64Array empty_array;
    array_init(MEMORY_TYPE_TARENA, &empty_array, 0);
    U64 *empty_first_ptr = array_first_ptr(&empty_array);
    TEST_ASSERT_NULL(empty_first_ptr);
}

void static test_array_performance_benchmark() {
    C8 pretty_buffer[PRETTY_BUFFER_SIZE] = {};

    TestU64Array array1;
    array_init(MEMORY_TYPE_TARENA, &array1, 16);

    U64 const num_operations = 1'000'000;

    F64 start_time = time_get_glfw_f64();
    for (U64 i = 0; i < num_operations; i++) { array_push(&array1, i); }
    F64 const insert_time = time_get_glfw_f64() - start_time;

    TEST_ASSERT_EQUAL_INT(num_operations, array1.count);
    unit_to_pretty_prefix_f("op/s", num_operations / insert_time, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    lli("Array Performance: Inserted %" PRIu64 " items in %.8fs (%s)", num_operations, insert_time, pretty_buffer);

    start_time = time_get_glfw_f64();
    for (U64 i = 0; i < num_operations; i++) {
        U64 const value = array_get(&array1, i);
        TEST_ASSERT_EQUAL_INT((S32)i, (S32)value);
    }
    F64 const get_time = time_get_glfw_f64() - start_time;
    unit_to_pretty_prefix_f("op/s", num_operations / get_time, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    lli("Array Performance: Retrieved %" PRIu64 " items in %.8fs (%s)", num_operations, get_time, pretty_buffer);

    start_time = time_get_glfw_f64();
    SZ count = 0;
    array_each(i, &array1) {
        unused(array1.data[i]);
        count++;
    };
    F64 const iterate_time = time_get_glfw_f64() - start_time;
    TEST_ASSERT_EQUAL_INT(num_operations, count);
    unit_to_pretty_prefix_f("op/s", num_operations / iterate_time, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    lli("Array Performance: Iterated %" PRIu64 " items in %.8fs (%s)", num_operations, iterate_time, pretty_buffer);

    start_time = time_get_glfw_f64();
    U64 const delete_count = (U64)((F64)array1.count * 0.001F);
    for (U64 i = 0; i < delete_count; i++) { array_remove_at(&array1, i); }
    F64 const remove_time = time_get_glfw_f64() - start_time;
    unit_to_pretty_prefix_f("op/s", (F64)(delete_count) / remove_time, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    lli("Array Performance: Removed %" PRIu64 " items in %.8fs (%s)", delete_count, remove_time, pretty_buffer);
    TEST_ASSERT_EQUAL_INT(num_operations - delete_count, array1.count);
}

void test_array() {
    RUN_TEST(test_array_init);
    RUN_TEST(test_array_push);
    RUN_TEST(test_array_pop);
    RUN_TEST(test_array_get);
    RUN_TEST(test_array_set);
    RUN_TEST(test_array_clear);
    RUN_TEST(test_array_last);
    RUN_TEST(test_array_first);
    RUN_TEST(test_array_last_ptr);
    RUN_TEST(test_array_first_ptr);
    RUN_TEST(test_array_empty);
    RUN_TEST(test_array_full);
    RUN_TEST(test_array_reserve);
    RUN_TEST(test_array_each);
    RUN_TEST(test_array_each_ptr);
    RUN_TEST(test_array_each_reverse);
    RUN_TEST(test_array_find);
    RUN_TEST(test_array_remove_at);
    RUN_TEST(test_array_remove_value);
    RUN_TEST(test_array_performance_benchmark);
}
