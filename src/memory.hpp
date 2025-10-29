#pragma once

#include "common.hpp"

enum MemoryType : U8 {
    MEMORY_TYPE_ARENA_PERMANENT,
    MEMORY_TYPE_ARENA_TRANSIENT,
    MEMORY_TYPE_ARENA_DEBUG,
    MEMORY_TYPE_ARENA_MATH,
    MEMORY_TYPE_COUNT,
};

// ===============================================================
// ============================ ARENA ============================
// ===============================================================

#define ARENA_MAX 128
#define ARENA_TIMELINE_MAX_COUNT 180

struct ArenaStats {
    SZ arena_count;
    SZ total_allocation_count;
    SZ total_capacity;
    SZ total_used;
    SZ max_used;
};

// INFO: We store these as F32 so that our debug timeline impl is easier. We would rather have a better type.
struct ArenaTimeline {
    F32 total_allocations_count[ARENA_TIMELINE_MAX_COUNT];
};

struct Arena {
    C8 const *name;
    SZ allocation_count;
    SZ capacity;
    SZ used;
    SZ max_used;
    void *memory;
};

struct ArenaAllocator {
    Arena *arenas[ARENA_MAX];
    SZ arena_count;
    SZ arena_capacity;
    ArenaStats previous_stats;
    ArenaTimeline timeline;
};

// ===============================================================
// =========================== MEMORY ============================
// ===============================================================

struct MemoryTypeSetup {
    BOOL verbose;
    SZ capacity;
};

struct MemorySetup {
    SZ alignment;
    MemoryTypeSetup per_type[MEMORY_TYPE_COUNT];
};

struct Memory {
    MemorySetup setup;
    ArenaAllocator arena_allocators[MEMORY_TYPE_COUNT];
};

void memory_init(MemorySetup setup);
void memory_post();
void memory_reset_type(MemoryType type);
void memory_quit();
ArenaStats memory_get_current_arena_stats(MemoryType type);
ArenaStats memory_get_previous_arena_stats(MemoryType type);
ArenaTimeline *memory_get_arena_timeline(MemoryType type);
C8 const *memory_type_to_cstr(MemoryType type);

void *memory_malloc(SZ size, MemoryType type);
void *memory_calloc(SZ count, SZ size, MemoryType type);
void *memory_realloc(void *ptr, SZ old_capacity, SZ new_capacity, MemoryType type);

void *memory_malloc_verbose(SZ size, MemoryType type, C8 const *file, S32 line);
void *memory_calloc_verbose(SZ count, SZ size, MemoryType type, C8 const *file, S32 line);
void *memory_realloc_verbose(void *ptr, SZ old_capacity, SZ new_capacity, MemoryType type, C8 const *file, S32 line);

#ifndef OURO_TRACE

// General
#define mm(t, size, type)                  (t)memory_malloc(size, type)
#define mc(t, count, size, type)           (t)memory_calloc(count, size, type)
#define mr(t, ptr, old_cap, new_cap, type) (t)memory_realloc(ptr, old_cap, new_cap, type)
// ARENA: Permanent
#define mmpa(t, size)                      (t)memory_malloc(size, MEMORY_TYPE_ARENA_PERMANENT)
#define mcpa(t, count, size)               (t)memory_calloc(count, size, MEMORY_TYPE_ARENA_PERMANENT)
#define mrpa(t, ptr, old_cap, new_cap)     (t)memory_realloc(ptr, old_cap, new_cap, MEMORY_TYPE_ARENA_PERMANENT)
// ARENA: Transient
#define mmta(t, size)                      (t)memory_malloc(size, MEMORY_TYPE_ARENA_TRANSIENT)
#define mcta(t, count, size)               (t)memory_calloc(count, size, MEMORY_TYPE_ARENA_TRANSIENT)
#define mrta(t, ptr, old_cap, new_cap)     (t)memory_realloc(ptr, old_cap, new_cap, MEMORY_TYPE_ARENA_TRANSIENT)
// ARENA: Debug
#define mmda(t, size)                      (t)memory_malloc(size, MEMORY_TYPE_ARENA_DEBUG)
#define mcda(t, count, size)               (t)memory_calloc(count, size, MEMORY_TYPE_ARENA_DEBUG)
#define mrda(t, ptr, old_cap, new_cap)     (t)memory_realloc(ptr, old_cap, new_cap, MEMORY_TYPE_ARENA_DEBUG)
// ARENA: Math
#define mmma(t, size)                      (t)memory_malloc(size, MEMORY_TYPE_ARENA_MATH)
#define mcma(t, count, size)               (t)memory_calloc(count, size, MEMORY_TYPE_ARENA_MATH)
#define mrma(t, ptr, old_cap, new_cap)     (t)memory_realloc(ptr, old_cap, new_cap, MEMORY_TYPE_ARENA_MATH)

#else

// General
#define mm(t, size, type)                  (t)memory_malloc_verbose(size, type, __FILE__, __LINE__)
#define mc(t, count, size, type)           (t)memory_calloc_verbose(count, size, type, __FILE__, __LINE__)
#define mr(t, ptr, old_cap, new_cap, type) (t)memory_realloc_verbose(ptr, old_cap, new_cap, type, __FILE__, __LINE__)
// ARENA: Permanent
#define mmpa(t, size)                      (t)memory_malloc_verbose(size, MEMORY_TYPE_ARENA_PERMANENT, __FILE__, __LINE__)
#define mcpa(t, count, size)               (t)memory_calloc_verbose(count, size, MEMORY_TYPE_ARENA_PERMANENT, __FILE__, __LINE__)
#define mrpa(t, ptr, old_cap, new_cap)     (t)memory_realloc_verbose(ptr, old_cap, new_cap, MEMORY_TYPE_ARENA_PERMANENT, __FILE__, __LINE__)
// ARENA: Transient
#define mmta(t, size)                      (t)memory_malloc_verbose(size, MEMORY_TYPE_ARENA_TRANSIENT, __FILE__, __LINE__)
#define mcta(t, count, size)               (t)memory_calloc_verbose(count, size, MEMORY_TYPE_ARENA_TRANSIENT, __FILE__, __LINE__)
#define mrta(t, ptr, old_cap, new_cap)     (t)memory_realloc_verbose(ptr, old_cap, new_cap, MEMORY_TYPE_ARENA_TRANSIENT, __FILE__, __LINE__)
// ARENA: Debug
#define mmda(t, size)                      (t)memory_malloc_verbose(size, MEMORY_TYPE_ARENA_DEBUG, __FILE__, __LINE__)
#define mcda(t, count, size)               (t)memory_calloc_verbose(count, size, MEMORY_TYPE_ARENA_DEBUG, __FILE__, __LINE__)
#define mrda(t, ptr, old_cap, new_cap)     (t)memory_realloc_verbose(ptr, old_cap, new_cap, MEMORY_TYPE_ARENA_DEBUG, __FILE__, __LINE__)
// ARENA: Math
#define mmma(t, size)                      (t)memory_malloc_verbose(size, MEMORY_TYPE_ARENA_MATH, __FILE__, __LINE__)
#define mcma(t, count, size)               (t)memory_calloc_verbose(count, size, MEMORY_TYPE_ARENA_MATH, __FILE__, __LINE__)
#define mrma(t, ptr, old_cap, new_cap)     (t)memory_realloc_verbose(ptr, old_cap, new_cap, MEMORY_TYPE_ARENA_MATH, __FILE__, __LINE__)

#endif
