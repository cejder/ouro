#pragma once

#include "common.hpp"
#include "memory.hpp"
#include "std.hpp"

#define RING_DECLARE(name, type) \
    struct name {                \
        MemoryType mem_type;     \
        SZ count;                \
        SZ capacity;             \
        SZ head;                 \
        SZ tail;                 \
        type *data;              \
    }

#define ring_init(mtype, ring, initial_capacity)                                                                                                      \
    do {                                                                                                                                              \
        (ring)->mem_type = (mtype);                                                                                                                   \
        (ring)->count = 0;                                                                                                                            \
        (ring)->capacity = (initial_capacity);                                                                                                        \
        (ring)->head = 0;                                                                                                                             \
        (ring)->tail = 0;                                                                                                                             \
        (ring)->data = (ring)->capacity > 0 ? (__typeof__((ring)->data))memory_malloc((ring)->capacity * sizeof(*(ring)->data), (mtype)) : nullptr; \
    } while (0)

#define ring_push(ring, item)                                     \
    do {                                                          \
        if ((ring)->count < (ring)->capacity) {                   \
            (ring)->data[(ring)->tail] = (item);                  \
            (ring)->tail = ((ring)->tail + 1) % (ring)->capacity; \
            (ring)->count++;                                      \
        } else if ((ring)->capacity > 0) {                        \
            (ring)->data[(ring)->tail] = (item);                  \
            (ring)->tail = ((ring)->tail + 1) % (ring)->capacity; \
            (ring)->head = ((ring)->head + 1) % (ring)->capacity; \
        }                                                         \
    } while (0)

#define ring_pop(ring)                                                                                                                                     \
    ((ring)->count > 0                                                                                                                                     \
         ? ((ring)->count--, (ring)->head = ((ring)->head + 1) % (ring)->capacity, (ring)->data[((ring)->head - 1 + (ring)->capacity) % (ring)->capacity]) \
         : (__typeof__(*(ring)->data)){0})

#define ring_peek(ring) ((ring)->count > 0 ? (ring)->data[(ring)->head] : (__typeof__(*(ring)->data)){0})

#define ring_peek_last(ring) ((ring)->count > 0 ? (ring)->data[((ring)->tail - 1 + (ring)->capacity) % (ring)->capacity] : (__typeof__(*(ring)->data)){0})

#define ring_get(ring, index) ((index) < (ring)->count ? (ring)->data[((ring)->head + (index)) % (ring)->capacity] : (__typeof__(*(ring)->data)){})

#define ring_set(ring, index, value)                                                                      \
    do {                                                                                                  \
        if ((index) < (ring)->count) (ring)->data[((ring)->head + (index)) % (ring)->capacity] = (value); \
    } while (0)

#define ring_clear(ring)   \
    do {                   \
        (ring)->count = 0; \
        (ring)->head = 0;  \
        (ring)->tail = 0;  \
    } while (0)

#define ring_empty(ring) ((ring)->count == 0)

#define ring_full(ring) ((ring)->count >= (ring)->capacity)

#define ring_each(it, ring) for (SZ it = 0; it < (ring)->count; it++)

#define ring_each_reverse(it, ring) for (SZ it = (ring)->count; it-- > 0;)

#define ring_find(ring, value, result_index)        \
    do {                                            \
        (result_index) = (SZ) - 1;                  \
        for (SZ _j = 0; _j < (ring)->count; ++_j) { \
            if (ring_get(ring, _j) == (value)) {    \
                (result_index) = _j;                \
                break;                              \
            }                                       \
        }                                           \
    } while (0)

#define ring_remove_at(ring, index)                                                      \
    do {                                                                                 \
        if ((index) < (ring)->count) {                                                   \
            if ((index) < (ring)->count / 2) {                                           \
                for (SZ _j = (index); _j > 0; --_j) {                                    \
                    SZ const _curr_idx = ((ring)->head + _j) % (ring)->capacity;         \
                    SZ const _prev_idx = ((ring)->head + _j - 1) % (ring)->capacity;     \
                    (ring)->data[_curr_idx] = (ring)->data[_prev_idx];                   \
                }                                                                        \
                (ring)->head = ((ring)->head + 1) % (ring)->capacity;                    \
            } else {                                                                     \
                for (SZ _j = (index); _j < (ring)->count - 1; ++_j) {                    \
                    SZ const _curr_idx = ((ring)->head + _j) % (ring)->capacity;         \
                    SZ const _next_idx = ((ring)->head + _j + 1) % (ring)->capacity;     \
                    (ring)->data[_curr_idx] = (ring)->data[_next_idx];                   \
                }                                                                        \
                (ring)->tail = ((ring)->tail - 1 + (ring)->capacity) % (ring)->capacity; \
            }                                                                            \
            (ring)->count--;                                                             \
        }                                                                                \
    } while (0)

#define ring_remove_value(ring, value)                                      \
    do {                                                                    \
        SZ found_index;                                                     \
        ring_find(ring, value, found_index);                                \
        if (found_index != (SZ) - 1) { ring_remove_at(ring, found_index); } \
    } while (0)
