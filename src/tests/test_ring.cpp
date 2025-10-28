#include "common.hpp"
#include "log.hpp"
#include "memory.hpp"
#include "ring.hpp"
#include "test.hpp"
#include "time.hpp"
#include "unit.hpp"

#include <unity.h>

RING_DECLARE(TestU64Ring, U64);

void static test_ring_init() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 0);
    TEST_ASSERT_EQUAL_INT(ring1.capacity, 0);
    TEST_ASSERT_EQUAL_INT(ring1.count, 0);
    TEST_ASSERT_EQUAL_INT(ring1.head, 0);
    TEST_ASSERT_EQUAL_INT(ring1.tail, 0);
    TEST_ASSERT_EQUAL_INT(ring1.mem_type, MEMORY_TYPE_TARENA);
    TEST_ASSERT_NULL(ring1.data);

    TestU64Ring ring2;
    ring_init(MEMORY_TYPE_TARENA, &ring2, 100);
    TEST_ASSERT_EQUAL_INT(ring2.capacity, 100);
    TEST_ASSERT_EQUAL_INT(ring2.count, 0);
    TEST_ASSERT_EQUAL_INT(ring2.head, 0);
    TEST_ASSERT_EQUAL_INT(ring2.tail, 0);
    TEST_ASSERT_EQUAL_INT(ring2.mem_type, MEMORY_TYPE_TARENA);
    TEST_ASSERT_NOT_NULL(ring2.data);
}

void static test_ring_push() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 3);
    ring_push(&ring1, 10);
    TEST_ASSERT_EQUAL_INT(ring1.capacity, 3);
    TEST_ASSERT_EQUAL_INT(ring1.count, 1);
    TEST_ASSERT_EQUAL_INT(ring1.head, 0);
    TEST_ASSERT_EQUAL_INT(ring1.tail, 1);
    TEST_ASSERT_NOT_NULL(ring1.data);
    TEST_ASSERT_TRUE(ring1.data[0] == 10);

    ring_push(&ring1, 20);
    TEST_ASSERT_EQUAL_INT(ring1.count, 2);
    TEST_ASSERT_EQUAL_INT(ring1.head, 0);
    TEST_ASSERT_EQUAL_INT(ring1.tail, 2);
    TEST_ASSERT_TRUE(ring1.data[0] == 10);
    TEST_ASSERT_TRUE(ring1.data[1] == 20);

    ring_push(&ring1, 30);
    TEST_ASSERT_EQUAL_INT(ring1.count, 3);
    TEST_ASSERT_EQUAL_INT(ring1.head, 0);
    TEST_ASSERT_EQUAL_INT(ring1.tail, 0);
    TEST_ASSERT_TRUE(ring1.data[0] == 10);
    TEST_ASSERT_TRUE(ring1.data[1] == 20);
    TEST_ASSERT_TRUE(ring1.data[2] == 30);

    ring_push(&ring1, 40);
    TEST_ASSERT_EQUAL_INT(ring1.count, 3);
    TEST_ASSERT_EQUAL_INT(ring1.head, 1);
    TEST_ASSERT_EQUAL_INT(ring1.tail, 1);
    TEST_ASSERT_TRUE(ring1.data[0] == 40);
    TEST_ASSERT_TRUE(ring1.data[1] == 20);
    TEST_ASSERT_TRUE(ring1.data[2] == 30);
}

void static test_ring_pop() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 3);
    ring_push(&ring1, 10);
    ring_push(&ring1, 20);
    ring_push(&ring1, 30);

    U64 const popped1 = ring_pop(&ring1);
    TEST_ASSERT_TRUE(popped1 == 10);
    TEST_ASSERT_EQUAL_INT(ring1.count, 2);
    TEST_ASSERT_EQUAL_INT(ring1.head, 1);

    U64 const popped2 = ring_pop(&ring1);
    TEST_ASSERT_TRUE(popped2 == 20);
    TEST_ASSERT_EQUAL_INT(ring1.count, 1);
    TEST_ASSERT_EQUAL_INT(ring1.head, 2);

    U64 const popped3 = ring_pop(&ring1);
    TEST_ASSERT_TRUE(popped3 == 30);
    TEST_ASSERT_EQUAL_INT(ring1.count, 0);
    TEST_ASSERT_EQUAL_INT(ring1.head, 0);

    U64 const popped_empty = ring_pop(&ring1);
    TEST_ASSERT_TRUE(popped_empty == 0);
    TEST_ASSERT_EQUAL_INT(ring1.count, 0);
}

void static test_ring_peek() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 3);

    U64 const peek_empty = ring_peek(&ring1);
    TEST_ASSERT_TRUE(peek_empty == 0);

    ring_push(&ring1, 10);
    ring_push(&ring1, 20);
    ring_push(&ring1, 30);

    U64 const peek1 = ring_peek(&ring1);
    TEST_ASSERT_TRUE(peek1 == 10);
    TEST_ASSERT_EQUAL_INT(ring1.count, 3);

    ring_pop(&ring1);
    U64 const peek2 = ring_peek(&ring1);
    TEST_ASSERT_TRUE(peek2 == 20);
    TEST_ASSERT_EQUAL_INT(ring1.count, 2);
}

void static test_ring_peek_last() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 3);

    U64 const peek_empty = ring_peek_last(&ring1);
    TEST_ASSERT_TRUE(peek_empty == 0);

    ring_push(&ring1, 10);
    ring_push(&ring1, 20);
    ring_push(&ring1, 30);

    U64 const peek_last = ring_peek_last(&ring1);
    TEST_ASSERT_TRUE(peek_last == 30);
    TEST_ASSERT_EQUAL_INT(ring1.count, 3);

    ring_push(&ring1, 40);
    U64 const peek_last2 = ring_peek_last(&ring1);
    TEST_ASSERT_TRUE(peek_last2 == 40);
    TEST_ASSERT_EQUAL_INT(ring1.count, 3);
}

void static test_ring_get() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 3);
    ring_push(&ring1, 10);
    ring_push(&ring1, 20);
    ring_push(&ring1, 30);

    U64 got = ring_get(&ring1, 0);
    TEST_ASSERT_TRUE(got == 10);
    got = ring_get(&ring1, 1);
    TEST_ASSERT_TRUE(got == 20);
    got = ring_get(&ring1, 2);
    TEST_ASSERT_TRUE(got == 30);
    got = ring_get(&ring1, 3);
    TEST_ASSERT_TRUE(got == 0);

    ring_push(&ring1, 40);
    got = ring_get(&ring1, 0);
    TEST_ASSERT_TRUE(got == 20);
    got = ring_get(&ring1, 1);
    TEST_ASSERT_TRUE(got == 30);
    got = ring_get(&ring1, 2);
    TEST_ASSERT_TRUE(got == 40);
}

void static test_ring_set() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 3);
    ring_push(&ring1, 0);
    ring_push(&ring1, 0);
    ring_push(&ring1, 0);

    ring_set(&ring1, 0, 10);
    ring_set(&ring1, 1, 20);
    ring_set(&ring1, 2, 30);

    U64 got = ring_get(&ring1, 0);
    TEST_ASSERT_TRUE(got == 10);
    got = ring_get(&ring1, 1);
    TEST_ASSERT_TRUE(got == 20);
    got = ring_get(&ring1, 2);
    TEST_ASSERT_TRUE(got == 30);
}

void static test_ring_clear() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 3);
    ring_push(&ring1, 10);
    ring_push(&ring1, 20);
    ring_clear(&ring1);
    TEST_ASSERT_EQUAL_INT(ring1.count, 0);
    TEST_ASSERT_EQUAL_INT(ring1.head, 0);
    TEST_ASSERT_EQUAL_INT(ring1.tail, 0);
}

void static test_ring_empty() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 3);

    BOOL empty = ring_empty(&ring1);
    TEST_ASSERT_TRUE(empty);

    ring_push(&ring1, 10);
    empty = ring_empty(&ring1);
    TEST_ASSERT_FALSE(empty);

    ring_clear(&ring1);
    TEST_ASSERT_EQUAL_INT(ring1.head, 0);
    TEST_ASSERT_EQUAL_INT(ring1.tail, 0);
    empty = ring_empty(&ring1);
    TEST_ASSERT_TRUE(empty);
}

void static test_ring_full() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 3);

    BOOL full = ring_full(&ring1);
    TEST_ASSERT_FALSE(full);

    ring_push(&ring1, 10);
    ring_push(&ring1, 20);
    full = ring_full(&ring1);
    TEST_ASSERT_FALSE(full);

    ring_push(&ring1, 30);
    full = ring_full(&ring1);
    TEST_ASSERT_TRUE(full);
}

void static test_ring_each() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 5);
    ring_push(&ring1, 10);
    ring_push(&ring1, 20);
    ring_push(&ring1, 30);

    SZ index = 0;
    ring_each(i, &ring1) {
        U64 const expected = (index + 1) * 10;
        TEST_ASSERT_EQUAL_INT(ring_get(&ring1, i), expected);
        index++;
    };
    TEST_ASSERT_EQUAL_INT(index, 3);
}

void static test_ring_each_reverse() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 5);
    ring_push(&ring1, 10);
    ring_push(&ring1, 20);
    ring_push(&ring1, 30);

    SZ const expected_values[] = {30, 20, 10};
    SZ index = 0;
    ring_each_reverse(i, &ring1) {
        U64 const value = ring_get(&ring1, i);
        TEST_ASSERT_EQUAL_INT(value, expected_values[index]);
        index++;
    };
    TEST_ASSERT_EQUAL_INT(index, 3);
}

void static test_ring_find() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 5);
    ring_push(&ring1, 10);
    ring_push(&ring1, 20);
    ring_push(&ring1, 30);
    ring_push(&ring1, 40);
    ring_push(&ring1, 50);

    SZ result = 0;
    ring_find(&ring1, 30, result);
    TEST_ASSERT_EQUAL_INT(result, 2);

    ring_find(&ring1, 50, result);
    TEST_ASSERT_EQUAL_INT(result, 4);

    ring_find(&ring1, 999, result);
    TEST_ASSERT_EQUAL_INT(result, (SZ)-1);

    ring_push(&ring1, 60);
    ring_find(&ring1, 20, result);
    TEST_ASSERT_EQUAL_INT(result, 0);
}

void static test_ring_remove_at() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 5);
    ring_push(&ring1, 10);
    ring_push(&ring1, 20);
    ring_push(&ring1, 30);
    ring_push(&ring1, 40);
    ring_push(&ring1, 50);

    ring_remove_at(&ring1, 1);
    TEST_ASSERT_EQUAL_INT(ring1.count, 4);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 0), 10);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 1), 30);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 2), 40);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 3), 50);

    ring_remove_at(&ring1, 3);
    TEST_ASSERT_EQUAL_INT(ring1.count, 3);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 0), 10);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 1), 30);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 2), 40);
}

void static test_ring_remove_value() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 5);
    ring_push(&ring1, 10);
    ring_push(&ring1, 20);
    ring_push(&ring1, 30);
    ring_push(&ring1, 40);
    ring_push(&ring1, 50);

    ring_remove_value(&ring1, 20);
    TEST_ASSERT_EQUAL_INT(ring1.count, 4);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 0), 10);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 1), 30);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 2), 40);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 3), 50);

    ring_remove_value(&ring1, 50);
    TEST_ASSERT_EQUAL_INT(ring1.count, 3);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 0), 10);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 1), 30);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 2), 40);
}

void static test_ring_wraparound() {
    TestU64Ring ring1;
    ring_init(MEMORY_TYPE_TARENA, &ring1, 3);

    ring_push(&ring1, 10);
    ring_push(&ring1, 20);
    ring_push(&ring1, 30);
    ring_push(&ring1, 40);
    ring_push(&ring1, 50);

    TEST_ASSERT_EQUAL_INT(ring1.count, 3);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 0), 30);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 1), 40);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 2), 50);

    U64 const popped = ring_pop(&ring1);
    TEST_ASSERT_TRUE(popped == 30);

    ring_push(&ring1, 60);
    TEST_ASSERT_EQUAL_INT(ring1.count, 3);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 0), 40);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 1), 50);
    TEST_ASSERT_EQUAL_INT(ring_get(&ring1, 2), 60);
}

void static test_ring_performance_benchmark() {
    C8 pretty_buffer[PRETTY_BUFFER_SIZE] = {};

    TestU64Ring ring1;

    U64 const capacity = 100'000;
    ring_init(MEMORY_TYPE_TARENA, &ring1, capacity);

    U64 const num_operations = 1'000'000;

    F64 start_time = time_get_glfw_f64();
    for (U64 i = 0; i < num_operations; i++) {
        ring_push(&ring1, i);
        U64 const value = ring_get(&ring1, i);
    }
    F64 const insert_time = time_get_glfw_f64() - start_time;

    TEST_ASSERT_EQUAL_INT(capacity, ring1.count);
    unit_to_pretty_prefix_f("op/s", num_operations / insert_time, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    lli("Ring Performance: Inserted %" PRIu64 " items in %.8fs (%s)", num_operations, insert_time, pretty_buffer);

    start_time = time_get_glfw_f64();
    for (U64 i = 0; i < num_operations; i++) {
        U64 const value = ring_get(&ring1, i);
        unused(value);
    }
    F64 const get_time = time_get_glfw_f64() - start_time;
    unit_to_pretty_prefix_f("op/s", num_operations / get_time, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    lli("Ring Performance: Retrieved %" PRIu64 " items in %.8fs (%s)", num_operations, get_time, pretty_buffer);

    start_time = time_get_glfw_f64();
    SZ count = 0;
    ring_each(i, &ring1) {
        unused(ring1.data[i]);
        count++;
    };
    F64 const iterate_time = time_get_glfw_f64() - start_time;
    TEST_ASSERT_EQUAL_INT(capacity, count);
    unit_to_pretty_prefix_f("op/s", num_operations / iterate_time, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    lli("Ring Performance: Iterated %" PRIu64 " items in %.8fs (%s)", capacity, iterate_time, pretty_buffer);

    start_time = time_get_glfw_f64();
    U64 const delete_count = capacity / 100;
    for (U64 i = 0; i < delete_count; i++) { ring_remove_at(&ring1, i); }
    F64 const remove_time = time_get_glfw_f64() - start_time;
    unit_to_pretty_prefix_f("op/s", (F64)(delete_count) / remove_time, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    lli("Ring Performance: Removed %" PRIu64 " items in %.8fs (%s)", delete_count, remove_time, pretty_buffer);
}

void test_ring() {
    RUN_TEST(test_ring_init);
    RUN_TEST(test_ring_push);
    RUN_TEST(test_ring_pop);
    RUN_TEST(test_ring_peek);
    RUN_TEST(test_ring_peek_last);
    RUN_TEST(test_ring_get);
    RUN_TEST(test_ring_set);
    RUN_TEST(test_ring_clear);
    RUN_TEST(test_ring_empty);
    RUN_TEST(test_ring_full);
    RUN_TEST(test_ring_each);
    RUN_TEST(test_ring_each_reverse);
    RUN_TEST(test_ring_find);
    RUN_TEST(test_ring_remove_at);
    RUN_TEST(test_ring_remove_value);
    RUN_TEST(test_ring_wraparound);
    RUN_TEST(test_ring_performance_benchmark);
}
