#pragma once

#include "common.hpp"
#include "memory.hpp"
#include "string.hpp"
#include "std.hpp"

U64 static inline hash_u64(U64 key) {
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53ULL;
    key ^= key >> 33;
    return key;
}

U32 static inline hash_u32(U32 key) {
    key ^= key >> 16;
    key *= 0x85ebca6b;
    key ^= key >> 13;
    key *= 0xc2b2ae35;
    key ^= key >> 16;
    return key;
}

U64 static inline hash_string(String const *s) {
    U64 hash = 0x9e3779b9;
    for (SZ i = 0; i < s->length; i++) {
        hash ^= (U64)s->data[i];
        hash *= 0x9e3779b9;
    }
    return hash;
}

U64 static inline hash_cstr(C8 const *s) {
    U64 hash = 0x9e3779b9;
    while (*s) {
        hash ^= (U64)*s;
        hash *= 0x9e3779b9;
        s++;
    }
    return hash;
}

SZ static inline i_next_power_of_two(SZ n) {
    if (n == 0) { return 1; }
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    n++;
    return n;
}

#define MAP_DECLARE(name, key_type, value_type, hash_fn, equal_fn)                                                                                         \
    struct name##Slot {                                                                                                                                    \
        key_type key;                                                                                                                                      \
        value_type value;                                                                                                                                  \
        BOOL occupied;                                                                                                                                     \
        BOOL tombstone;                                                                                                                                    \
        U64 hash;                                                                                                                                          \
    };                                                                                                                                                     \
                                                                                                                                                           \
    struct name {                                                                                                                                          \
        SZ count;                                                                                                                                          \
        SZ capacity;                                                                                                                                       \
        SZ tombstone_count;                                                                                                                                \
        MemoryType memory_type;                                                                                                                            \
        name##Slot *slots;                                                                                                                                 \
    };                                                                                                                                                     \
                                                                                                                                                           \
    void static inline name##_init(name *map, MemoryType type, SZ initial_capacity) {                                                                      \
        SZ const cap = i_next_power_of_two(initial_capacity == 0 ? 16 : initial_capacity);                                                                   \
        map->count = 0;                                                                                                                                    \
        map->tombstone_count = 0;                                                                                                                          \
        map->capacity = cap;                                                                                                                               \
        map->memory_type = type;                                                                                                                           \
        map->slots = mc(name##Slot *, cap, sizeof(name##Slot), type);                                                                                      \
        ou_memset(map->slots, 0, cap * sizeof(name##Slot));                                                                                                \
    }                                                                                                                                                      \
                                                                                                                                                           \
    void static inline name##_insert(name *map, key_type key, value_type value) {                                                                          \
        if ((map->count + map->tombstone_count) * 4 >= map->capacity * 3) {                                                                                \
            name const old_map = *map;                                                                                                                     \
            SZ const new_capacity = map->capacity * 2;                                                                                                     \
            map->capacity = new_capacity;                                                                                                                  \
            map->count = 0;                                                                                                                                \
            map->tombstone_count = 0;                                                                                                                      \
            map->slots = mc(name##Slot *, new_capacity, sizeof(name##Slot), map->memory_type);                                                             \
            ou_memset(map->slots, 0, new_capacity * sizeof(name##Slot));                                                                                   \
            for (SZ i = 0; i < old_map.capacity; i++) {                                                                                                    \
                if (old_map.slots[i].occupied) {                                                                                                           \
                    SZ index = old_map.slots[i].hash & (map->capacity - 1);                                                                                \
                    while (map->slots[index].occupied) { index = (index + 1) & (map->capacity - 1); }                                                      \
                    map->slots[index] = old_map.slots[i];                                                                                                  \
                    map->count++;                                                                                                                          \
                }                                                                                                                                          \
            }                                                                                                                                              \
        }                                                                                                                                                  \
        SZ const capacity_mask = map->capacity - 1;                                                                                                        \
        name##Slot *slots = map->slots;                                                                                                                    \
        U64 const hash = hash_fn(key);                                                                                                                     \
        SZ index = hash & capacity_mask;                                                                                                                   \
        SZ first_tombstone = SZ_MAX;                                                                                                                       \
        while (slots[index].occupied || slots[index].tombstone) {                                                                                          \
            if (slots[index].occupied && slots[index].hash == hash && equal_fn(slots[index].key, key)) {                                                  \
                slots[index].value = value;                                                                                                                \
                return;                                                                                                                                    \
            }                                                                                                                                              \
            if (slots[index].tombstone && first_tombstone == SZ_MAX) { first_tombstone = index; }                                                          \
            index = (index + 1) & capacity_mask;                                                                                                           \
        }                                                                                                                                                  \
        SZ const target = (first_tombstone != SZ_MAX) ? first_tombstone : index;                                                                           \
        if (slots[target].tombstone) { map->tombstone_count--; }                                                                                           \
        slots[target].key = key;                                                                                                                           \
        slots[target].value = value;                                                                                                                       \
        slots[target].occupied = true;                                                                                                                     \
        slots[target].tombstone = false;                                                                                                                   \
        slots[target].hash = hash;                                                                                                                         \
        map->count++;                                                                                                                                      \
    }                                                                                                                                                      \
                                                                                                                                                           \
    value_type static inline *name##_get(name *map, key_type key) {                                                                                        \
        if (map->capacity == 0) { return NULL; }                                                                                                           \
        SZ const capacity_mask = map->capacity - 1;                                                                                                        \
        name##Slot const *slots = map->slots;                                                                                                              \
        U64 const hash = hash_fn(key);                                                                                                                     \
        SZ index = hash & capacity_mask;                                                                                                                   \
        SZ const start_index = index;                                                                                                                      \
        do {                                                                                                                                               \
            name##Slot const *slot = &slots[index];                                                                                                        \
            if (slot->occupied) {                                                                                                                          \
                if (slot->hash == hash && equal_fn(slot->key, key)) { return &map->slots[index].value; }                                                  \
            } else if (!slot->tombstone) {                                                                                                                 \
                return NULL;                                                                                                                               \
            }                                                                                                                                              \
            index = (index + 1) & capacity_mask;                                                                                                           \
        } while (index != start_index);                                                                                                                    \
        return NULL;                                                                                                                                       \
    }                                                                                                                                                      \
                                                                                                                                                           \
    BOOL static inline name##_has(name *map, key_type key) {                                                                                               \
        return name##_get(map, key) != NULL;                                                                                                               \
    }                                                                                                                                                      \
                                                                                                                                                           \
    void static inline name##_remove(name *map, key_type key) {                                                                                            \
        if (map->capacity == 0) { return; }                                                                                                                \
        SZ const capacity_mask = map->capacity - 1;                                                                                                        \
        name##Slot *slots = map->slots;                                                                                                                    \
        U64 const hash = hash_fn(key);                                                                                                                     \
        SZ index = hash & capacity_mask;                                                                                                                   \
        SZ const start_index = index;                                                                                                                      \
        do {                                                                                                                                               \
            if (slots[index].occupied && slots[index].hash == hash && equal_fn(slots[index].key, key)) {                                                  \
                slots[index].occupied = false;                                                                                                             \
                slots[index].tombstone = true;                                                                                                             \
                map->count--;                                                                                                                              \
                map->tombstone_count++;                                                                                                                    \
                return;                                                                                                                                    \
            }                                                                                                                                              \
            if (!slots[index].occupied && !slots[index].tombstone) { return; }                                                                             \
            index = (index + 1) & capacity_mask;                                                                                                           \
        } while (index != start_index);                                                                                                                    \
    }                                                                                                                                                      \
                                                                                                                                                           \
    void static inline name##_clear(name *map) {                                                                                                           \
        map->count = 0;                                                                                                                                    \
        map->tombstone_count = 0;                                                                                                                          \
        for (SZ i = 0; i < map->capacity; i++) {                                                                                                           \
            map->slots[i].occupied = false;                                                                                                                \
            map->slots[i].tombstone = false;                                                                                                               \
        }                                                                                                                                                  \
    }                                                                                                                                                      \
                                                                                                                                                           \
    F32 static inline name##_load_factor(name *map) {                                                                                                      \
        return map->capacity > 0 ? (F32)(map->count + map->tombstone_count) / (F32)map->capacity : 0.0f;                                                   \
    }

#define MAP_HASH_U64(key) hash_u64((U64)(key))
#define MAP_HASH_U32(key) hash_u64((U64)(key))
#define MAP_HASH_STRING(key) hash_string((key))
#define MAP_HASH_CSTR(key) hash_cstr((key))
#define MAP_EQUAL_U64(a, b) ((a) == (b))
#define MAP_EQUAL_U32(a, b) ((a) == (b))
#define MAP_EQUAL_STRING(a, b) string_equals((a), (b))
#define MAP_EQUAL_CSTR(a, b) (ou_strcmp((a), (b)) == 0)

#define MAP_EACH(map, key_var, value_var)                                            \
    for (SZ _i = 0, _found = 0; _i < (map)->capacity && _found < (map)->count; _i++) \
        if ((map)->slots[_i].occupied && (_found++, (key_var) = (map)->slots[_i].key, (value_var) = (map)->slots[_i].value, true))

#define MAP_EACH_PTR(map, key_ptr_var, value_ptr_var)                                \
    for (SZ _i = 0, _found = 0; _i < (map)->capacity && _found < (map)->count; _i++) \
        if ((map)->slots[_i].occupied && (_found++, (key_ptr_var) = &(map)->slots[_i].key, (value_ptr_var) = &(map)->slots[_i].value, true))
