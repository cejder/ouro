#pragma once

#include "common.hpp"

#define ARENA_MAX 128
#define MEMORY_TIMELINE_MAX_COUNT 180

enum MemoryType : U8 {
    MEMORY_TYPE_PARENA,
    MEMORY_TYPE_TARENA,
    MEMORY_TYPE_DARENA,
    MEMORY_TYPE_MARENA,
    MEMORY_TYPE_COUNT,
};

struct MemoryArenaStats {
    SZ arena_count;
    SZ total_allocation_count;
    SZ total_capacity;
    SZ total_used;
    SZ max_used;
};

// INFO: We store these as F32 so that our debug timeline impl is easier.
// We would rather have a better type.
struct MemoryArenaTimeline {
    F32 total_allocations_count[MEMORY_TIMELINE_MAX_COUNT];
};

struct MemoryArena {
    C8 const *name;
    SZ allocation_count;
    SZ capacity;
    SZ used;
    SZ max_used;
    void *memory;
};

struct MemoryArenaAllocator {
    MemoryType type;
    MemoryArena *arenas[ARENA_MAX];
    SZ arena_count;
    SZ arena_capacity;
    MemoryArenaStats last_stats;
    MemoryArenaTimeline timeline;
};

struct MemorySetup {
    SZ permanent_arena_capacity;
    SZ transient_arena_capacity;
    SZ debug_arena_capacity;
    SZ math_arena_capacity;
    BOOL verbose_permanent_arena;
    BOOL verbose_transient_arena;
    BOOL verbose_debug_arena;
    BOOL verbose_math_arena;
    SZ memory_alignment;
};

struct Memory {
    MemoryArenaAllocator arena_allocators[MEMORY_TYPE_COUNT];
    SZ memory_alignment;
    BOOL enabled_verbose_logging[MEMORY_TYPE_COUNT];
};

void memory_init(MemorySetup setup);
void memory_post();
void memory_reset_arena(MemoryType type);
void memory_quit();
MemoryArenaStats memory_get_current_arena_stats(MemoryType type);
MemoryArenaStats memory_get_last_arena_stats(MemoryType type);
MemoryArenaTimeline *memory_get_timeline(MemoryType type);
C8 const *memory_type_to_cstr(MemoryType type);
void *memory_oumalloc(SZ size, MemoryType type);
void *memory_oucalloc(SZ count, SZ size, MemoryType type);
void *memory_ourealloc(void *ptr, SZ old_capacity, SZ new_capacity, MemoryType type);
void *memory_oumalloc_verbose(SZ size, MemoryType type, C8 const *file, S32 line);
void *memory_oucalloc_verbose(SZ count, SZ size, MemoryType type, C8 const *file, S32 line);
void *memory_ourealloc_verbose(void *ptr, SZ old_capacity, SZ new_capacity, MemoryType type, C8 const *file, S32 line);

#ifdef OURO_TRACE

// General
#define mm(t, size, type)                  (t)memory_oumalloc_verbose(size, type, __FILE__, __LINE__)
#define mc(t, count, size, type)           (t)memory_oucalloc_verbose(count, size, type, __FILE__, __LINE__)
#define mr(t, ptr, old_cap, new_cap, type) (t)memory_ourealloc_verbose(ptr, old_cap, new_cap, type, __FILE__, __LINE__)
// Permanent
#define mmpa(t, size)                      (t)memory_oumalloc_verbose(size, MEMORY_TYPE_PARENA, __FILE__, __LINE__)
#define mcpa(t, count, size)               (t)memory_oucalloc_verbose(count, size, MEMORY_TYPE_PARENA, __FILE__, __LINE__)
#define mrpa(t, ptr, old_cap, new_cap)     (t)memory_ourealloc_verbose(ptr, old_cap, new_cap, MEMORY_TYPE_PARENA, __FILE__, __LINE__)
// Transient
#define mmta(t, size)                      (t)memory_oumalloc_verbose(size, MEMORY_TYPE_TARENA, __FILE__, __LINE__)
#define mcta(t, count, size)               (t)memory_oucalloc_verbose(count, size, MEMORY_TYPE_TARENA, __FILE__, __LINE__)
#define mrta(t, ptr, old_cap, new_cap)     (t)memory_ourealloc_verbose(ptr, old_cap, new_cap, MEMORY_TYPE_TARENA, __FILE__, __LINE__)
// Debug
#define mmda(t, size)                      (t)memory_oumalloc_verbose(size, MEMORY_TYPE_DARENA, __FILE__, __LINE__)
#define mcda(t, count, size)               (t)memory_oucalloc_verbose(count, size, MEMORY_TYPE_DARENA, __FILE__, __LINE__)
#define mrda(t, ptr, old_cap, new_cap)     (t)memory_ourealloc_verbose(ptr, old_cap, new_cap, MEMORY_TYPE_DARENA, __FILE__, __LINE__)
// Math
#define mmma(t, size)                      (t)memory_oumalloc_verbose(size, MEMORY_TYPE_MARENA, __FILE__, __LINE__)
#define mcma(t, count, size)               (t)memory_oucalloc_verbose(count, size, MEMORY_TYPE_MARENA, __FILE__, __LINE__)
#define mrma(t, ptr, old_cap, new_cap)     (t)memory_ourealloc_verbose(ptr, old_cap, new_cap, MEMORY_TYPE_MARENA, __FILE__, __LINE__)

#else

// General
#define mm(t, size, type)                  (t)memory_oumalloc(size, type)
#define mc(t, count, size, type)           (t)memory_oucalloc(count, size, type)
#define mr(t, ptr, old_cap, new_cap, type) (t)memory_ourealloc(ptr, old_cap, new_cap, type)
// Permanent
#define mmpa(t, size)                      (t)memory_oumalloc(size, MEMORY_TYPE_PARENA)
#define mcpa(t, count, size)               (t)memory_oucalloc(count, size, MEMORY_TYPE_PARENA)
#define mrpa(t, ptr, old_cap, new_cap)     (t)memory_ourealloc(ptr, old_cap, new_cap, MEMORY_TYPE_PARENA)
// Transient
#define mmta(t, size)                      (t)memory_oumalloc(size, MEMORY_TYPE_TARENA)
#define mcta(t, count, size)               (t)memory_oucalloc(count, size, MEMORY_TYPE_TARENA)
#define mrta(t, ptr, old_cap, new_cap)     (t)memory_ourealloc(ptr, old_cap, new_cap, MEMORY_TYPE_TARENA)
// Debug
#define mmda(t, size)                      (t)memory_oumalloc(size, MEMORY_TYPE_DARENA)
#define mcda(t, count, size)               (t)memory_oucalloc(count, size, MEMORY_TYPE_DARENA)
#define mrda(t, ptr, old_cap, new_cap)     (t)memory_ourealloc(ptr, old_cap, new_cap, MEMORY_TYPE_DARENA)
// Math
#define mmma(t, size)                      (t)memory_oumalloc(size, MEMORY_TYPE_MARENA)
#define mcma(t, count, size)               (t)memory_oucalloc(count, size, MEMORY_TYPE_MARENA)
#define mrma(t, ptr, old_cap, new_cap)     (t)memory_ourealloc(ptr, old_cap, new_cap, MEMORY_TYPE_MARENA)

#endif
