#include "memory.hpp"
#include "log.hpp"
#include "std.hpp"

#include <stdlib.h>
#include <glm/common.hpp>

Memory static i_memory = {};

C8 static const *i_memory_type_to_cstr[MEMORY_TYPE_COUNT] = {
    "Permanent",
    "Transient",
    "Debug",
    "Math",
};

Arena static *i_memory_arena_create(SZ capacity) {
    if (capacity < 1) {
        lle("Arena size must be at least 1");
        return nullptr;
    }

    auto *arena = (Arena *)calloc(1, sizeof(Arena));  // NOLINT
    if (!arena) {
        lle("Could not allocate memory for arena");
        return nullptr;
    }

#if defined(_WIN32)
    arena->memory = _aligned_malloc(capacity, i_memory.memory_alignment);  // NOLINT
#else
    arena->memory = aligned_alloc(i_memory.memory_alignment, capacity);  // NOLINT
#endif
    if (!arena->memory) {
        free(arena);  // NOLINT
        lle("Could not allocate memory for arena base");
        return nullptr;
    }

    arena->capacity = capacity;

    return arena;
}

void static i_memory_arena_destroy(Arena *arena) {
    free(arena->memory);  // NOLINT
    free(arena);          // NOLINT
}

void static *i_memory_arena_alloc(Arena *arena, SZ size) {
    if (arena->used + size > arena->capacity) {
        llt("Arena is full, %zu + %zu > %zu", arena->used, size, arena->capacity);
        return nullptr;
    }

    void *ptr       = (U8 *)arena->memory + arena->used;
    arena->used    += size;
    arena->max_used = glm::max(arena->max_used, arena->used);
    arena->allocation_count++;

    return ptr;
}

void static *i_memory_arena_allocator_alloc(ArenaAllocator *allocator, SZ size) {
    if (size < 1) {
        lle("Allocation size must be at least 1");
        return nullptr;
    }

    SZ const aligned_size = (size + i_memory.memory_alignment - 1) & ~(i_memory.memory_alignment - 1);

    if (aligned_size > allocator->arena_capacity) {
        lle("Allocation size is too big %zu > %zu", size, allocator->arena_capacity);
        return nullptr;
    }

    for (SZ i = 0; i < allocator->arena_count; ++i) {
        void *ptr = i_memory_arena_alloc(allocator->arenas[i], aligned_size);
        if (ptr) { return ptr; }
    }

    if (allocator->arena_count >= ARENA_MAX) {
        lle("Arena allocator is full");
        return nullptr;
    }

    Arena *arena = i_memory_arena_create(allocator->arena_capacity);
    if (!arena) {
        lle("Could not allocate memory for arena");
        return nullptr;
    }

    allocator->arenas[allocator->arena_count++] = arena;

    return i_memory_arena_alloc(arena, aligned_size);
}

void memory_init(MemorySetup setup) {
    if (setup.permanent_arena_capacity < 1) {
        lle("Permanent arena capacity must be at least 1 byte");
        return;
    }

    if (setup.transient_arena_capacity < 1) {
        lle("Transient arena capacity must be at least 1 byte");
        return;
    }

    if (setup.debug_arena_capacity < 1) {
        lle("Debug arena capacity must be at least 1 byte");
        return;
    }

    if (setup.math_arena_capacity < 1) {
        lle("Math arena capacity must be at least 1 byte");
        return;
    }

    if (setup.memory_alignment == 0) {
        lle("Memory alignment has to be set or not 0 (%zu)", setup.memory_alignment);
        return;
    }

    i_memory.memory_alignment = setup.memory_alignment;

    i_memory.arena_allocators[MEMORY_TYPE_ARENA_PERMANENT].arenas[0] = i_memory_arena_create(setup.permanent_arena_capacity);
    if (!i_memory.arena_allocators[MEMORY_TYPE_ARENA_PERMANENT].arenas[0]) {
        lle("Could not allocate memory for permanent arena allocator");
        return;
    }
    i_memory.arena_allocators[MEMORY_TYPE_ARENA_PERMANENT].arena_count    = 1;
    i_memory.arena_allocators[MEMORY_TYPE_ARENA_PERMANENT].arena_capacity = setup.permanent_arena_capacity;

    i_memory.arena_allocators[MEMORY_TYPE_ARENA_TRANSIENT].arenas[0] = i_memory_arena_create(setup.transient_arena_capacity);
    if (!i_memory.arena_allocators[MEMORY_TYPE_ARENA_TRANSIENT].arenas[0]) {
        lle("Could not allocate memory for transient arena allocator");
        return;
    }
    i_memory.arena_allocators[MEMORY_TYPE_ARENA_TRANSIENT].arena_count    = 1;
    i_memory.arena_allocators[MEMORY_TYPE_ARENA_TRANSIENT].arena_capacity = setup.transient_arena_capacity;

    i_memory.arena_allocators[MEMORY_TYPE_ARENA_DEBUG].arenas[0] = i_memory_arena_create(setup.debug_arena_capacity);
    if (!i_memory.arena_allocators[MEMORY_TYPE_ARENA_DEBUG].arenas[0]) {
        lle("Could not allocate memory for debug arena allocator");
        return;
    }
    i_memory.arena_allocators[MEMORY_TYPE_ARENA_DEBUG].arena_count    = 1;
    i_memory.arena_allocators[MEMORY_TYPE_ARENA_DEBUG].arena_capacity = setup.debug_arena_capacity;

    i_memory.arena_allocators[MEMORY_TYPE_ARENA_MATH].arenas[0] = i_memory_arena_create(setup.math_arena_capacity);
    if (!i_memory.arena_allocators[MEMORY_TYPE_ARENA_MATH].arenas[0]) {
        lle("Could not allocate memory for math arena allocator");
        return;
    }
    i_memory.arena_allocators[MEMORY_TYPE_ARENA_MATH].arena_count    = 1;
    i_memory.arena_allocators[MEMORY_TYPE_ARENA_MATH].arena_capacity = setup.math_arena_capacity;

    // NOTE: We don't free anything here when we fail because we don't care cleaning up since we're exiting anyway.

    i_memory.enabled_verbose_logging[MEMORY_TYPE_ARENA_PERMANENT] = setup.permanent_arena_verbose;
    i_memory.enabled_verbose_logging[MEMORY_TYPE_ARENA_TRANSIENT] = setup.transient_arena_verbose;
    i_memory.enabled_verbose_logging[MEMORY_TYPE_ARENA_DEBUG] = setup.debug_arena_verbose;
    i_memory.enabled_verbose_logging[MEMORY_TYPE_ARENA_MATH] = setup.math_arena_verbose;
}

void memory_quit() {
    for (const auto &arena_allocator : i_memory.arena_allocators) {
        for (SZ j = 0; j < arena_allocator.arena_count; ++j) { i_memory_arena_destroy(arena_allocator.arenas[j]); }
    }
}

C8 static const *i_reduce_filepath(C8 const *file) {
    C8 const *last_file = file;
    for (SZ i = 0; i < ou_strlen(file); ++i) {
        if (file[i] == '/' || file[i] == '\\') { last_file = &file[i + 1]; }
    }
    return last_file;
}

void *memory_oumalloc_verbose(SZ size, MemoryType type, C8 const *file, S32 line) {
    if (i_memory.enabled_verbose_logging[type]) {
        lltty("(%s) Mallocating %zu bytes of memory at %s:%d", i_memory_type_to_cstr[type], size, i_reduce_filepath(file), line);
    }

    return memory_oumalloc(size, type);
}

void *memory_oumalloc(SZ size, MemoryType type) {
    return i_memory_arena_allocator_alloc(&i_memory.arena_allocators[type], size);
}

void *memory_oucalloc_verbose(SZ count, SZ size, MemoryType type, C8 const *file, S32 line) {
    if (i_memory.enabled_verbose_logging[type]) {
        lltty("(%s) Callocating %zu bytes of memory at %s:%d", i_memory_type_to_cstr[type], count * size, i_reduce_filepath(file), line);
    }
    return memory_oucalloc(count, size, type);
}

void *memory_oucalloc(SZ count, SZ size, MemoryType type) {
    void *ptr = memory_oumalloc(count * size, type);
    if (!ptr) { return nullptr; }
    ou_memset(ptr, 0, count * size);
    return ptr;
}

void *memory_ourealloc(void *ptr, SZ old_capacity, SZ new_capacity, MemoryType type) {
    void *new_ptr = memory_oumalloc(new_capacity, type);
    if (!new_ptr) { return nullptr; }
    ou_memmove(new_ptr, ptr, old_capacity);
    return new_ptr;
}

void *memory_ourealloc_verbose(void *ptr, SZ old_capacity, SZ new_capacity, MemoryType type, C8 const *file, S32 line) {
    if (i_memory.enabled_verbose_logging[type]) {
        lltty("(%s) Reallocating from %zu to %zu bytes of memory at %s:%d", i_memory_type_to_cstr[type], old_capacity, new_capacity, i_reduce_filepath(file), line);
    }
    return memory_ourealloc(ptr, old_capacity, new_capacity, type);
}

void memory_post() {
    for (S32 i = 0; i < MEMORY_TYPE_COUNT; ++i) {
        ArenaAllocator *a = &i_memory.arena_allocators[i];
        a->last_stats = memory_get_current_arena_stats((MemoryType)i);

        for (SZ j = 0; j < ARENA_TIMELINE_MAX_COUNT - 1; ++j) { a->timeline.total_allocations_count[j] = a->timeline.total_allocations_count[j + 1]; }

        a->timeline.total_allocations_count[ARENA_TIMELINE_MAX_COUNT - 1] = (F32)a->last_stats.total_allocation_count;
    }

    for (SZ i = 0; i < i_memory.arena_allocators[MEMORY_TYPE_ARENA_TRANSIENT].arena_count; ++i) {
        Arena *arena = i_memory.arena_allocators[MEMORY_TYPE_ARENA_TRANSIENT].arenas[i];
        if (arena) {
            free(arena->memory);  // NOLINT
            free(arena);          // NOLINT
        }
        i_memory.arena_allocators[MEMORY_TYPE_ARENA_TRANSIENT].arenas[i] = nullptr;
    }
    i_memory.arena_allocators[MEMORY_TYPE_ARENA_TRANSIENT].arena_count = 0;
}

void memory_reset_arena(MemoryType type) {
    ArenaAllocator *allocator = &i_memory.arena_allocators[type];
    for (SZ i = 0; i < allocator->arena_count; ++i) {
        Arena *arena      = allocator->arenas[i];
        arena->used             = 0;
        arena->allocation_count = 0;
    }
}

ArenaStats memory_get_current_arena_stats(MemoryType type) {
    ArenaStats stats = {};
    ArenaAllocator *arena_allocator = &i_memory.arena_allocators[type];

    for (SZ i = 0; i < arena_allocator->arena_count; ++i) {
        Arena const *arena = arena_allocator->arenas[i];
        stats.arena_count++;
        stats.total_allocation_count += arena->allocation_count;
        stats.total_capacity         += arena->capacity;
        stats.total_used             += arena->used;
        stats.max_used                = glm::max(stats.max_used, arena->max_used);
    }

    return stats;
}

ArenaStats memory_get_last_arena_stats(MemoryType type) {
    return i_memory.arena_allocators[type].last_stats;
}

ArenaTimeline *memory_get_timeline(MemoryType type) {
    return &i_memory.arena_allocators[type].timeline;
}

C8 const *memory_type_to_cstr(MemoryType type) {
    return i_memory_type_to_cstr[type];
}
