#pragma once

#include "common.hpp"
#include "memory.hpp"
#include "log.hpp"

#include <raylib.h>

#define ARRAY_DECLARE(name, type) \
    struct name {                 \
        MemoryType mem_type;      \
        SZ count;                 \
        SZ capacity;              \
        type *data;               \
    }

#define array_init(mtype, array, initial_capacity)                                                                                                         \
    do {                                                                                                                                                   \
        (array)->mem_type = (mtype);                                                                                                                       \
        (array)->count = 0;                                                                                                                                \
        (array)->capacity = (initial_capacity);                                                                                                            \
        (array)->data = (array)->capacity > 0 ? (__typeof__((array)->data))memory_malloc((array)->capacity * sizeof(*(array)->data), (mtype)) : nullptr; \
    } while (0)

#define array_push(array, item)                                                                                                                            \
    do {                                                                                                                                                   \
        if ((array)->count >= (array)->capacity) {                                                                                                         \
            SZ const _new_capacity = (array)->capacity == 0 ? 8 : (array)->capacity * 2;                                                                   \
            __typeof__(*(array)->data) *_new_data = (__typeof__((array)->data))memory_malloc(_new_capacity * sizeof(*(array)->data), (array)->mem_type); \
            if (!_new_data) {                                                                                                                              \
                lle("Failed to allocate memory for array expansion");                                                                                      \
                break;                                                                                                                                     \
            }                                                                                                                                              \
            if ((array)->data && (array)->count > 0) {                                                                                                     \
                for (SZ _j = 0; _j < (array)->count; _j++) { _new_data[_j] = (array)->data[_j]; }                                                          \
            }                                                                                                                                              \
            (array)->data = _new_data;                                                                                                                     \
            (array)->capacity = _new_capacity;                                                                                                             \
        }                                                                                                                                                  \
        (array)->data[(array)->count++] = item;                                                                                                            \
    } while (0)

#define array_push_multiple(array, items, items_count)                                                                                                         \
    do {                                                                                                                                                       \
        SZ const _items_count = (items_count);                                                                                                                 \
        if (_items_count > 0) {                                                                                                                                \
            SZ const _needed_capacity = (array)->count + _items_count;                                                                                         \
            if (_needed_capacity > (array)->capacity) {                                                                                                        \
                SZ _new_capacity = (array)->capacity == 0 ? 8 : (array)->capacity;                                                                             \
                while (_new_capacity < _needed_capacity) { _new_capacity *= 2; }                                                                               \
                __typeof__(*(array)->data) *_new_data = (__typeof__((array)->data))memory_malloc(_new_capacity * sizeof(*(array)->data), (array)->mem_type); \
                if (!_new_data) {                                                                                                                              \
                    lle("Failed to allocate memory for array expansion");                                                                                      \
                    break;                                                                                                                                     \
                }                                                                                                                                              \
                if ((array)->data && (array)->count > 0) {                                                                                                     \
                    for (SZ _j = 0; _j < (array)->count; _j++) { _new_data[_j] = (array)->data[_j]; }                                                          \
                }                                                                                                                                              \
                (array)->data = _new_data;                                                                                                                     \
                (array)->capacity = _new_capacity;                                                                                                             \
            }                                                                                                                                                  \
            for (SZ _j = 0; _j < _items_count; _j++) { (array)->data[(array)->count + _j] = (items)[_j]; }                                                     \
            (array)->count += _items_count;                                                                                                                    \
        }                                                                                                                                                      \
    } while (0)

#define array_pop(array) ((array)->count > 0 ? (array)->data[--(array)->count] : (__typeof__(*(array)->data)){0})

#define array_get(array, index) ((array)->data[index])

#define array_get_ptr(array, index) (&(array)->data[index])

#define array_set(array, index, value) \
    do { (array)->data[index] = (value); } while (0)

#define array_clear(array) \
    do { (array)->count = 0; } while (0)

#define array_last(array) ((array)->count > 0 ? (array)->data[(array)->count - 1] : (__typeof__(*(array)->data)){0})

#define array_last_ptr(array) ((array)->count > 0 ? &(array)->data[(array)->count - 1] : nullptr)

#define array_first(array) ((array)->count > 0 ? (array)->data[0] : (__typeof__(*(array)->data)){0})

#define array_first_ptr(array) ((array)->count > 0 ? &(array)->data[0] : nullptr)

#define array_empty(array) ((array)->count == 0)

#define array_full(array) ((array)->count >= (array)->capacity)

#define array_reserve(array, new_capacity)                                                                                                                  \
    do {                                                                                                                                                    \
        if ((new_capacity) > (array)->capacity) {                                                                                                           \
            __typeof__(*(array)->data) *_new_data = (__typeof__((array)->data))memory_malloc((new_capacity) * sizeof(*(array)->data), (array)->mem_type); \
            if ((array)->data && (array)->count > 0) {                                                                                                      \
                for (SZ _j = 0; _j < (array)->count; ++_j) { _new_data[_j] = (array)->data[_j]; }                                                           \
            }                                                                                                                                               \
            (array)->data = _new_data;                                                                                                                      \
            (array)->capacity = (new_capacity);                                                                                                             \
        }                                                                                                                                                   \
    } while (0)

#define array_each(it, array) for (SZ it = 0; it < (array)->count; it++)

#define array_each_ptr(ptr, array) for (__typeof__(*(array)->data) *ptr = (array)->data; ptr < (array)->data + (array)->count; ptr++)

#define array_each_reverse(it, array) for (SZ it = (array)->count; it-- > 0;)

#define array_find(array, value, result_index)       \
    do {                                             \
        (result_index) = (SZ) - 1;                   \
        for (SZ _j = 0; _j < (array)->count; ++_j) { \
            if ((array)->data[_j] == (value)) {      \
                (result_index) = _j;                 \
                break;                               \
            }                                        \
        }                                            \
    } while (0)

#define array_remove_at(array, index)                                                                           \
    do {                                                                                                        \
        if ((index) < (array)->count) {                                                                         \
            for (SZ _j = (index); _j < (array)->count - 1; ++_j) { (array)->data[_j] = (array)->data[_j + 1]; } \
            (array)->count--;                                                                                   \
        }                                                                                                       \
    } while (0)

#define array_remove_value(array, value)                                      \
    do {                                                                      \
        SZ found_index;                                                       \
        array_find(array, value, found_index);                                \
        if (found_index != (SZ) - 1) { array_remove_at(array, found_index); } \
    } while (0)


// Common array declarations

ARRAY_DECLARE(Vector2Array, Vector2);
ARRAY_DECLARE(Vector3Array, Vector3);
ARRAY_DECLARE(MatrixArray,  Matrix);
ARRAY_DECLARE(SZArray,      SZ);
ARRAY_DECLARE(U32Array,     U32);
ARRAY_DECLARE(U64Array,     U64);
ARRAY_DECLARE(BOOLArray,    BOOL);
ARRAY_DECLARE(CstrArray ,   C8*);
ARRAY_DECLARE(EIDArray,     EID);
