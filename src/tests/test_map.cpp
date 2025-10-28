#include "log.hpp"
#include "map.hpp"
#include "test.hpp"
#include "time.hpp"
#include "unit.hpp"

#include <unity.h>

MAP_DECLARE(TestU64Map, U64, S32, MAP_HASH_U64, MAP_EQUAL_U64);
MAP_DECLARE(TestStringMap, String *, S32, MAP_HASH_STRING, MAP_EQUAL_STRING);
MAP_DECLARE(TestCstrMap, C8 const *, S32, MAP_HASH_CSTR, MAP_EQUAL_CSTR);

void static test_hashmap_u64_init() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 0);
    TEST_ASSERT_EQUAL_INT(0, map.count);
    TEST_ASSERT_EQUAL_INT(16, map.capacity);
    TEST_ASSERT_EQUAL_INT(0, map.tombstone_count);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_TARENA, map.memory_type);
    TEST_ASSERT_NOT_NULL(map.slots);

    TestU64Map map_custom;
    TestU64Map_init(&map_custom, MEMORY_TYPE_TARENA, 32);
    TEST_ASSERT_EQUAL_INT(0, map_custom.count);
    TEST_ASSERT_EQUAL_INT(32, map_custom.capacity);
    TEST_ASSERT_EQUAL_INT(0, map_custom.tombstone_count);
    TEST_ASSERT_NOT_NULL(map_custom.slots);
}

void static test_hashmap_u64_insert_get() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 0);

    TestU64Map_insert(&map, 42, 100);
    TEST_ASSERT_EQUAL_INT(1, map.count);

    S32 *value = TestU64Map_get(&map, 42);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(100, *value);

    S32 *missing = TestU64Map_get(&map, 999);
    TEST_ASSERT_NULL(missing);
}

void static test_hashmap_u64_insert_update() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 0);

    TestU64Map_insert(&map, 42, 100);
    TEST_ASSERT_EQUAL_INT(1, map.count);

    S32 *value = TestU64Map_get(&map, 42);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(100, *value);

    TestU64Map_insert(&map, 42, 200);
    TEST_ASSERT_EQUAL_INT(1, map.count);

    value = TestU64Map_get(&map, 42);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(200, *value);
}

void static test_hashmap_u64_has() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 0);

    TEST_ASSERT_FALSE(TestU64Map_has(&map, 42));

    TestU64Map_insert(&map, 42, 100);
    TEST_ASSERT_TRUE(TestU64Map_has(&map, 42));
    TEST_ASSERT_FALSE(TestU64Map_has(&map, 999));
}

void static test_hashmap_u64_remove() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 0);

    TestU64Map_insert(&map, 42, 100);
    TestU64Map_insert(&map, 84, 200);
    TEST_ASSERT_EQUAL_INT(2, map.count);
    TEST_ASSERT_EQUAL_INT(0, map.tombstone_count);

    TestU64Map_remove(&map, 42);
    TEST_ASSERT_EQUAL_INT(1, map.count);
    TEST_ASSERT_EQUAL_INT(1, map.tombstone_count);
    TEST_ASSERT_FALSE(TestU64Map_has(&map, 42));
    TEST_ASSERT_TRUE(TestU64Map_has(&map, 84));

    TestU64Map_remove(&map, 999);
    TEST_ASSERT_EQUAL_INT(1, map.count);
    TEST_ASSERT_EQUAL_INT(1, map.tombstone_count);
}

void static test_hashmap_u64_clear() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 0);

    TestU64Map_insert(&map, 42, 100);
    TestU64Map_insert(&map, 84, 200);
    TestU64Map_remove(&map, 42);
    TEST_ASSERT_EQUAL_INT(1, map.count);
    TEST_ASSERT_EQUAL_INT(1, map.tombstone_count);

    TestU64Map_clear(&map);
    TEST_ASSERT_EQUAL_INT(0, map.count);
    TEST_ASSERT_EQUAL_INT(0, map.tombstone_count);
    TEST_ASSERT_FALSE(TestU64Map_has(&map, 84));
}

void static test_hashmap_u64_load_factor() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 4);

    TEST_ASSERT_EQUAL_FLOAT(0.0F, TestU64Map_load_factor(&map));

    TestU64Map_insert(&map, 1, 10);
    TEST_ASSERT_EQUAL_FLOAT(0.25F, TestU64Map_load_factor(&map));

    TestU64Map_insert(&map, 2, 20);
    TEST_ASSERT_EQUAL_FLOAT(0.5F, TestU64Map_load_factor(&map));

    TestU64Map_remove(&map, 1);
    TEST_ASSERT_EQUAL_FLOAT(0.5F, TestU64Map_load_factor(&map));
}

void static test_hashmap_u64_rehashing() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 4);

    TestU64Map_insert(&map, 1, 10);
    TestU64Map_insert(&map, 2, 20);
    TestU64Map_insert(&map, 3, 30);

    SZ const capacity_before_4th = map.capacity;
    TestU64Map_insert(&map, 4, 40);

    TEST_ASSERT_TRUE(map.capacity > capacity_before_4th);
    TEST_ASSERT_EQUAL_INT(4, map.count);
    TEST_ASSERT_EQUAL_INT(0, map.tombstone_count);

    S32 *val1 = TestU64Map_get(&map, 1);
    S32 *val2 = TestU64Map_get(&map, 2);
    S32 *val3 = TestU64Map_get(&map, 3);
    S32 *val4 = TestU64Map_get(&map, 4);
    TEST_ASSERT_NOT_NULL(val1);
    TEST_ASSERT_NOT_NULL(val2);
    TEST_ASSERT_NOT_NULL(val3);
    TEST_ASSERT_NOT_NULL(val4);
    TEST_ASSERT_EQUAL_INT(10, *val1);
    TEST_ASSERT_EQUAL_INT(20, *val2);
    TEST_ASSERT_EQUAL_INT(30, *val3);
    TEST_ASSERT_EQUAL_INT(40, *val4);
}

void static test_hashmap_u64_iteration() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 0);

    TestU64Map_insert(&map, 10, 100);
    TestU64Map_insert(&map, 20, 200);
    TestU64Map_insert(&map, 30, 300);

    SZ count = 0;
    S32 sum_keys = 0;
    S32 sum_values = 0;

    U64 key = 0;
    S32 value = 0;
    MAP_EACH(&map, key, value) {
        count++;
        sum_keys += (S32)key;
        sum_values += value;
    }
    unused(key);
    unused(value);

    TEST_ASSERT_EQUAL_INT(3, count);
    TEST_ASSERT_EQUAL_INT(60, sum_keys);
    TEST_ASSERT_EQUAL_INT(600, sum_values);
}

void static test_hashmap_u64_iteration_ptr() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 0);

    TestU64Map_insert(&map, 10, 100);
    TestU64Map_insert(&map, 20, 200);
    TestU64Map_insert(&map, 30, 300);

    SZ count = 0;
    U64 const *key_ptr = nullptr;
    S32 *value_ptr = nullptr;
    MAP_EACH_PTR(&map, key_ptr, value_ptr) {
        count++;
        *value_ptr += 1;
    }

    TEST_ASSERT_EQUAL_INT(3, count);
    TEST_ASSERT_EQUAL_INT(101, *TestU64Map_get(&map, 10));
    TEST_ASSERT_EQUAL_INT(201, *TestU64Map_get(&map, 20));
    TEST_ASSERT_EQUAL_INT(301, *TestU64Map_get(&map, 30));
}

void static test_hashmap_string_operations() {
    TestStringMap map;
    TestStringMap_init(&map, MEMORY_TYPE_TARENA, 0);

    String *key1 = TS("hello");
    String *key2 = TS("world");
    String *key3 = TS("test");

    TestStringMap_insert(&map, key1, 100);
    TestStringMap_insert(&map, key2, 200);
    TestStringMap_insert(&map, key3, 300);

    TEST_ASSERT_EQUAL_INT(3, map.count);
    TEST_ASSERT_TRUE(TestStringMap_has(&map, key1));
    TEST_ASSERT_TRUE(TestStringMap_has(&map, key2));
    TEST_ASSERT_TRUE(TestStringMap_has(&map, key3));

    S32 *val1 = TestStringMap_get(&map, key1);
    S32 *val2 = TestStringMap_get(&map, key2);
    S32 *val3 = TestStringMap_get(&map, key3);
    TEST_ASSERT_NOT_NULL(val1);
    TEST_ASSERT_NOT_NULL(val2);
    TEST_ASSERT_NOT_NULL(val3);
    TEST_ASSERT_EQUAL_INT(100, *val1);
    TEST_ASSERT_EQUAL_INT(200, *val2);
    TEST_ASSERT_EQUAL_INT(300, *val3);

    TestStringMap_remove(&map, key2);
    TEST_ASSERT_EQUAL_INT(2, map.count);
    TEST_ASSERT_EQUAL_INT(1, map.tombstone_count);
    TEST_ASSERT_FALSE(TestStringMap_has(&map, key2));

    String *missing = TS("missing");
    TEST_ASSERT_FALSE(TestStringMap_has(&map, missing));
    TEST_ASSERT_NULL(TestStringMap_get(&map, missing));
}

void static test_hashmap_cstr_operations() {
    TestCstrMap map;
    TestCstrMap_init(&map, MEMORY_TYPE_TARENA, 0);

    C8 const *key1 = "apple";
    C8 const *key2 = "banana";
    C8 const *key3 = "cherry";

    TestCstrMap_insert(&map, key1, 10);
    TestCstrMap_insert(&map, key2, 20);
    TestCstrMap_insert(&map, key3, 30);

    TEST_ASSERT_EQUAL_INT(3, map.count);
    TEST_ASSERT_TRUE(TestCstrMap_has(&map, key1));
    TEST_ASSERT_TRUE(TestCstrMap_has(&map, key2));
    TEST_ASSERT_TRUE(TestCstrMap_has(&map, key3));

    S32 *val1 = TestCstrMap_get(&map, key1);
    S32 *val2 = TestCstrMap_get(&map, key2);
    S32 *val3 = TestCstrMap_get(&map, key3);
    TEST_ASSERT_NOT_NULL(val1);
    TEST_ASSERT_NOT_NULL(val2);
    TEST_ASSERT_NOT_NULL(val3);
    TEST_ASSERT_EQUAL_INT(10, *val1);
    TEST_ASSERT_EQUAL_INT(20, *val2);
    TEST_ASSERT_EQUAL_INT(30, *val3);

    TestCstrMap_remove(&map, key2);
    TEST_ASSERT_EQUAL_INT(2, map.count);
    TEST_ASSERT_EQUAL_INT(1, map.tombstone_count);
    TEST_ASSERT_FALSE(TestCstrMap_has(&map, key2));

    TEST_ASSERT_FALSE(TestCstrMap_has(&map, "missing"));
    TEST_ASSERT_NULL(TestCstrMap_get(&map, "missing"));
}

void static test_hashmap_edge_cases() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 0);

    S32 *empty_get = TestU64Map_get(&map, 42);
    TEST_ASSERT_NULL(empty_get);
    TEST_ASSERT_FALSE(TestU64Map_has(&map, 42));

    TestU64Map_remove(&map, 42);
    TEST_ASSERT_EQUAL_INT(0, map.count);

    TestU64Map_clear(&map);
    TEST_ASSERT_EQUAL_INT(0, map.count);

    TestU64Map empty_map;
    TestU64Map_init(&empty_map, MEMORY_TYPE_TARENA, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, TestU64Map_load_factor(&empty_map));
}

void static test_hashmap_collision_handling() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 4);

    U64 const key1 = 0;
    U64 const key2 = 4;

    TestU64Map_insert(&map, key1, 100);
    TestU64Map_insert(&map, key2, 200);

    TEST_ASSERT_EQUAL_INT(2, map.count);
    TEST_ASSERT_TRUE(TestU64Map_has(&map, key1));
    TEST_ASSERT_TRUE(TestU64Map_has(&map, key2));

    S32 *val1 = TestU64Map_get(&map, key1);
    S32 *val2 = TestU64Map_get(&map, key2);
    TEST_ASSERT_NOT_NULL(val1);
    TEST_ASSERT_NOT_NULL(val2);
    TEST_ASSERT_EQUAL_INT(100, *val1);
    TEST_ASSERT_EQUAL_INT(200, *val2);

    TestU64Map_remove(&map, key1);
    TEST_ASSERT_FALSE(TestU64Map_has(&map, key1));
    TEST_ASSERT_TRUE(TestU64Map_has(&map, key2));
    TEST_ASSERT_EQUAL_INT(200, *TestU64Map_get(&map, key2));
}

void static test_hashmap_tombstone_reuse() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 8);

    TestU64Map_insert(&map, 1, 10);
    TestU64Map_insert(&map, 2, 20);
    SZ const tombstones_before = map.tombstone_count;

    TestU64Map_remove(&map, 1);
    TEST_ASSERT_EQUAL_INT(1, map.count);
    TEST_ASSERT_EQUAL_INT(tombstones_before + 1, map.tombstone_count);

    SZ const tombstones_after_remove = map.tombstone_count;
    TestU64Map_insert(&map, 3, 30);

    TEST_ASSERT_EQUAL_INT(2, map.count);
    TEST_ASSERT_TRUE(map.tombstone_count <= tombstones_after_remove);

    TEST_ASSERT_FALSE(TestU64Map_has(&map, 1));
    TEST_ASSERT_TRUE(TestU64Map_has(&map, 2));
    TEST_ASSERT_TRUE(TestU64Map_has(&map, 3));
    TEST_ASSERT_EQUAL_INT(20, *TestU64Map_get(&map, 2));
    TEST_ASSERT_EQUAL_INT(30, *TestU64Map_get(&map, 3));
}

void static test_hashmap_stress_many_insertions() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 4);

    for (U64 i = 0; i < 100; i++) { TestU64Map_insert(&map, i, (S32)(i * 10)); }

    TEST_ASSERT_EQUAL_INT(100, map.count);

    for (U64 i = 0; i < 100; i++) {
        S32 *value = TestU64Map_get(&map, i);
        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_EQUAL_INT((S32)(i * 10), *value);
    }

    for (U64 i = 200; i < 300; i++) {
        TEST_ASSERT_FALSE(TestU64Map_has(&map, i));
        TEST_ASSERT_NULL(TestU64Map_get(&map, i));
    }
}

void static test_hashmap_stress_remove_all() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 4);

    for (U64 i = 0; i < 20; i++) { TestU64Map_insert(&map, i, (S32)(i * 10)); }
    TEST_ASSERT_EQUAL_INT(20, map.count);

    for (U64 i = 0; i < 20; i++) { TestU64Map_remove(&map, i); }
    TEST_ASSERT_EQUAL_INT(0, map.count);
    TEST_ASSERT_EQUAL_INT(20, map.tombstone_count);

    for (U64 i = 0; i < 20; i++) {
        TEST_ASSERT_FALSE(TestU64Map_has(&map, i));
        TEST_ASSERT_NULL(TestU64Map_get(&map, i));
    }
}

void static test_hashmap_stress_hash_collisions() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 8);

    U64 const keys[] = {0, 8, 16, 24, 32, 40, 48, 56};
    S32 const values[] = {10, 20, 30, 40, 50, 60, 70, 80};

    for (SZ i = 0; i < 8; i++) { TestU64Map_insert(&map, keys[i], values[i]); }

    TEST_ASSERT_EQUAL_INT(8, map.count);

    for (SZ i = 0; i < 8; i++) {
        S32 *value = TestU64Map_get(&map, keys[i]);
        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_EQUAL_INT(values[i], *value);
    }

    TestU64Map_remove(&map, keys[0]);
    TestU64Map_remove(&map, keys[4]);

    TEST_ASSERT_EQUAL_INT(6, map.count);
    TEST_ASSERT_EQUAL_INT(2, map.tombstone_count);

    for (SZ i = 1; i < 4; i++) {
        S32 *value = TestU64Map_get(&map, keys[i]);
        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_EQUAL_INT(values[i], *value);
    }
    for (SZ i = 5; i < 8; i++) {
        S32 *value = TestU64Map_get(&map, keys[i]);
        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_EQUAL_INT(values[i], *value);
    }

    TEST_ASSERT_FALSE(TestU64Map_has(&map, keys[0]));
    TEST_ASSERT_FALSE(TestU64Map_has(&map, keys[4]));
}

void static test_hashmap_stress_update_values() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 0);

    for (U64 i = 0; i < 50; i++) { TestU64Map_insert(&map, i, (S32)i); }
    for (U64 i = 0; i < 50; i++) { TestU64Map_insert(&map, i, (S32)(i * 100)); }

    TEST_ASSERT_EQUAL_INT(50, map.count);

    for (U64 i = 0; i < 50; i++) {
        S32 *value = TestU64Map_get(&map, i);
        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_EQUAL_INT((S32)(i * 100), *value);
    }
}

void static test_hashmap_edge_case_empty_operations() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 0);

    TEST_ASSERT_NULL(TestU64Map_get(&map, 999));
    TEST_ASSERT_FALSE(TestU64Map_has(&map, 999));
    TestU64Map_remove(&map, 999);
    TEST_ASSERT_EQUAL_INT(0, map.count);
    TEST_ASSERT_EQUAL_INT(0, map.tombstone_count);

    TestU64Map_clear(&map);
    TEST_ASSERT_EQUAL_INT(0, map.count);
    TEST_ASSERT_EQUAL_INT(0, map.tombstone_count);
}

void static test_hashmap_iteration_with_tombstones() {
    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 0);

    for (U64 i = 0; i < 10; i++) { TestU64Map_insert(&map, i, (S32)(i * 10)); }

    TestU64Map_remove(&map, 2);
    TestU64Map_remove(&map, 5);
    TestU64Map_remove(&map, 8);

    SZ iteration_count = 0;
    U64 key = 0;
    S32 value = 0;
    MAP_EACH(&map, key, value) {
        iteration_count++;
        TEST_ASSERT_NOT_EQUAL(2, key);
        TEST_ASSERT_NOT_EQUAL(5, key);
        TEST_ASSERT_NOT_EQUAL(8, key);
        TEST_ASSERT_EQUAL_INT((S32)(key * 10), value);
    }
    unused(key);
    unused(value);

    TEST_ASSERT_EQUAL_INT(7, iteration_count);
    TEST_ASSERT_EQUAL_INT(7, map.count);
}

void static test_hashmap_performance_benchmark() {
    C8 pretty_buffer[PRETTY_BUFFER_SIZE] = {};

    TestU64Map map;
    TestU64Map_init(&map, MEMORY_TYPE_TARENA, 16);

    U64 const num_operations = 1'000'000;

    F64 start_time = time_get_glfw_f64();
    for (U64 i = 0; i < num_operations; i++) { TestU64Map_insert(&map, i, (S32)(i * 2)); }
    F64 const insert_time = time_get_glfw_f64() - start_time;

    TEST_ASSERT_EQUAL_INT(num_operations, map.count);
    unit_to_pretty_prefix_f("op/s", num_operations / insert_time, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    lli("HashMap Performance: Inserted %" PRIu64 " items in %.8fs (%s)", num_operations, insert_time, pretty_buffer);

    start_time = time_get_glfw_f64();
    for (U64 i = 0; i < num_operations; i++) {
        S32 *value = TestU64Map_get(&map, i);
        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_EQUAL_INT((S32)(i * 2), *value);
    }
    F64 const get_time = time_get_glfw_f64() - start_time;
    unit_to_pretty_prefix_f("op/s", num_operations / get_time, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    lli("HashMap Performance: Retrieved %" PRIu64 " items in %.8fs (%s)", num_operations, get_time, pretty_buffer);

    start_time = time_get_glfw_f64();
    for (U64 i = 0; i < num_operations; i++) { TEST_ASSERT_TRUE(TestU64Map_has(&map, i)); }
    F64 const has_time = time_get_glfw_f64() - start_time;
    unit_to_pretty_prefix_f("op/s", num_operations / has_time, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    lli("HashMap Performance: Checked %" PRIu64 " items in %.8fs (%s)", num_operations, has_time, pretty_buffer);

    start_time = time_get_glfw_f64();
    for (U64 i = 0; i < num_operations; i++) { TestU64Map_insert(&map, i, (S32)(i * 10)); }
    F64 const update_time = time_get_glfw_f64() - start_time;
    unit_to_pretty_prefix_f("op/s", num_operations / update_time, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    lli("HashMap Performance: Updated %" PRIu64 " items in %.8fs (%s)", num_operations, update_time, pretty_buffer);

    start_time = time_get_glfw_f64();
    SZ count = 0;
    U64 key = 0;
    S32 value = 0;
    MAP_EACH(&map, key, value) {
        count++;
    }
    unused(key);
    unused(value);
    F64 const iterate_time = time_get_glfw_f64() - start_time;
    TEST_ASSERT_EQUAL_INT(num_operations, count);
    unit_to_pretty_prefix_f("op/s", num_operations / iterate_time, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    lli("HashMap Performance: Iterated %" PRIu64 " items in %.8fs (%s)", num_operations, iterate_time, pretty_buffer);

    start_time = time_get_glfw_f64();
    for (U64 i = 0; i < num_operations / 2; i++) { TestU64Map_remove(&map, i * 2); }
    F64 const remove_time = time_get_glfw_f64() - start_time;
    unit_to_pretty_prefix_f("op/s", ((F32)num_operations / 2) / remove_time, pretty_buffer, PRETTY_BUFFER_SIZE, UNIT_PREFIX_GIGA);
    lli("HashMap Performance: Removed %" PRIu64 " items in %.8fs (%s)", num_operations / 2, remove_time, pretty_buffer);

    TEST_ASSERT_EQUAL_INT(num_operations / 2, map.count);
    TEST_ASSERT_EQUAL_INT(num_operations / 2, map.tombstone_count);

    F32 const load_factor = TestU64Map_load_factor(&map);
    lli("HashMap Performance: Final capacity=%zu, count=%zu, tombstones=%zu, load_factor=%.2f", map.capacity, map.count, map.tombstone_count, load_factor);
}

void test_map() {
    RUN_TEST(test_hashmap_u64_init);
    RUN_TEST(test_hashmap_u64_insert_get);
    RUN_TEST(test_hashmap_u64_insert_update);
    RUN_TEST(test_hashmap_u64_has);
    RUN_TEST(test_hashmap_u64_remove);
    RUN_TEST(test_hashmap_u64_clear);
    RUN_TEST(test_hashmap_u64_load_factor);
    RUN_TEST(test_hashmap_u64_rehashing);
    RUN_TEST(test_hashmap_u64_iteration);
    RUN_TEST(test_hashmap_u64_iteration_ptr);
    RUN_TEST(test_hashmap_string_operations);
    RUN_TEST(test_hashmap_cstr_operations);
    RUN_TEST(test_hashmap_edge_cases);
    RUN_TEST(test_hashmap_collision_handling);
    RUN_TEST(test_hashmap_tombstone_reuse);
    RUN_TEST(test_hashmap_stress_many_insertions);
    RUN_TEST(test_hashmap_stress_remove_all);
    RUN_TEST(test_hashmap_stress_hash_collisions);
    RUN_TEST(test_hashmap_stress_update_values);
    RUN_TEST(test_hashmap_edge_case_empty_operations);
    RUN_TEST(test_hashmap_iteration_with_tombstones);
    RUN_TEST(test_hashmap_performance_benchmark);
}
