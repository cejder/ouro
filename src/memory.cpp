#include "memory.hpp"
#include "log.hpp"
#include "std.hpp"

#include <raylib.h>
#include <stdlib.h>
#include <glm/common.hpp>

Memory static i_memory = {};

C8 static const *i_type_to_cstr[MEMORY_TYPE_COUNT] = {
    "Permanent Arena",
    "Transient Arena",
    "Debug Arena",
    "Math Arena",
};

void static i_free(void* ptr) {
#if defined(_WIN32)
    _aligned_free(ptr); // NOLINT
#else
    free(ptr); // NOLINT
#endif
}

// ===============================================================
// ============================ ARENA ============================
// ===============================================================

Arena static *i_arena_create(SZ capacity) {
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
    arena->memory = aligned_alloc(i_memory.setup.alignment, capacity);  // NOLINT
#endif
    if (!arena->memory) {
        i_free(arena);  // NOLINT
        lle("Could not allocate memory for arena base");
        return nullptr;
    }

    arena->capacity = capacity;

    return arena;
}

void static i_arena_destroy(Arena *arena) {
    i_free(arena->memory);  // NOLINT
    i_free(arena);          // NOLINT
}

void static *i_arena_alloc(Arena *arena, SZ size) {
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

void static *i_arena_allocator_alloc(ArenaAllocator *allocator, SZ size) {
    if (size < 1) {
        lle("Allocation size must be at least 1");
        return nullptr;
    }

    SZ const aligned_size = (size + i_memory.setup.alignment - 1) & ~(i_memory.setup.alignment - 1);

    if (aligned_size > allocator->arena_capacity) {
        lle("Allocation size is too big %zu > %zu", size, allocator->arena_capacity);
        return nullptr;
    }

    for (SZ i = 0; i < allocator->arena_count; ++i) {
        void *ptr = i_arena_alloc(allocator->arenas[i], aligned_size);
        if (ptr) { return ptr; }
    }

    if (allocator->arena_count >= ARENA_MAX) {
        lle("Arena allocator is full");
        return nullptr;
    }

    Arena *arena = i_arena_create(allocator->arena_capacity);
    if (!arena) {
        lle("Could not allocate memory for arena");
        return nullptr;
    }

    allocator->arenas[allocator->arena_count++] = arena;

    return i_arena_alloc(arena, aligned_size);
}

// ===============================================================
// =========================== MEMORY ============================
// ===============================================================

void memory_init(MemorySetup setup) {
    i_memory.setup = setup;

    if (i_memory.setup.alignment == 0) {
        lle("Memory alignment has to be set or not 0 (%zu)", i_memory.setup.alignment);
        return;
    }

    for (SZ i = 0; i < MEMORY_TYPE_COUNT; ++i) {
        MemoryTypeSetup *s = &i_memory.setup.per_type[i];

        if (s->capacity < 1) {
            lle("\"%s\" allocator has less than 1 byte capacity", i_type_to_cstr[i]);
            return;
        }

        i_memory.arena_allocators[i].arenas[0] = i_arena_create(s->capacity);
        if (!i_memory.arena_allocators[i].arenas[0]) {
            lle("Could not allocate memory for \"%s\" allocator", i_type_to_cstr[i]);
            return;
        }
        i_memory.arena_allocators[i].arena_count    = 1;
        i_memory.arena_allocators[i].arena_capacity = s->capacity;
    }
}

void memory_quit() {
    for (const auto &arena_allocator : i_memory.arena_allocators) {
        for (SZ j = 0; j < arena_allocator.arena_count; ++j) { i_arena_destroy(arena_allocator.arenas[j]); }
    }
}

void *memory_malloc_verbose(SZ size, MemoryType type, C8 const *file, S32 line) {
    if (i_memory.setup.per_type[type].verbose) {
        lltty("(%s) Mallocating %zu bytes of memory at %s:%d", i_type_to_cstr[type], size, GetFileName(file), line);
    }
    return memory_malloc(size, type);
}
void *memory_malloc(SZ size, MemoryType type) {
    return i_arena_allocator_alloc(&i_memory.arena_allocators[type], size);
}

void *memory_calloc_verbose(SZ count, SZ size, MemoryType type, C8 const *file, S32 line) {
    if (i_memory.setup.per_type[type].verbose) {
        lltty("(%s) Callocating %zu bytes of memory at %s:%d", i_type_to_cstr[type], count * size, GetFileName(file), line);
    }
    return memory_calloc(count, size, type);
}
void *memory_calloc(SZ count, SZ size, MemoryType type) {
    void *ptr = memory_malloc(count * size, type);
    if (!ptr) { return nullptr; }
    ou_memset(ptr, 0, count * size);
    return ptr;
}

void *memory_realloc(void *ptr, SZ old_capacity, SZ new_capacity, MemoryType type) {
    void *new_ptr = memory_malloc(new_capacity, type);
    if (!new_ptr) { return nullptr; }
    ou_memmove(new_ptr, ptr, old_capacity);
    return new_ptr;
}
void *memory_realloc_verbose(void *ptr, SZ old_capacity, SZ new_capacity, MemoryType type, C8 const *file, S32 line) {
    if (i_memory.setup.per_type[type].verbose) {
        lltty("(%s) Reallocating from %zu to %zu bytes of memory at %s:%d", i_type_to_cstr[type], old_capacity, new_capacity, GetFileName(file), line);
    }
    return memory_realloc(ptr, old_capacity, new_capacity, type);
}

void memory_post() {
    for (S32 i = 0; i < MEMORY_TYPE_COUNT; ++i) {
        ArenaAllocator *a = &i_memory.arena_allocators[i];
        a->previous_stats = memory_get_current_arena_stats((MemoryType)i);

        for (SZ j = 0; j < ARENA_TIMELINE_MAX_COUNT - 1; ++j) { a->timeline.total_allocations_count[j] = a->timeline.total_allocations_count[j + 1]; }

        a->timeline.total_allocations_count[ARENA_TIMELINE_MAX_COUNT - 1] = (F32)a->previous_stats.total_allocation_count;
    }

    for (SZ i = 0; i < i_memory.arena_allocators[MEMORY_TYPE_ARENA_TRANSIENT].arena_count; ++i) {
        Arena *arena = i_memory.arena_allocators[MEMORY_TYPE_ARENA_TRANSIENT].arenas[i];
        if (arena) {
            i_free(arena->memory);  // NOLINT
            i_free(arena);          // NOLINT
        }
        i_memory.arena_allocators[MEMORY_TYPE_ARENA_TRANSIENT].arenas[i] = nullptr;
    }
    i_memory.arena_allocators[MEMORY_TYPE_ARENA_TRANSIENT].arena_count = 0;
}

void memory_reset_type(MemoryType type) {
    ArenaAllocator *allocator = &i_memory.arena_allocators[type];
    for (SZ i = 0; i < allocator->arena_count; ++i) {
        Arena *arena            = allocator->arenas[i];
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

ArenaStats memory_get_previous_arena_stats(MemoryType type) {
    return i_memory.arena_allocators[type].previous_stats;
}

ArenaTimeline *memory_get_arena_timeline(MemoryType type) {
    return &i_memory.arena_allocators[type].timeline;
}

C8 const *memory_type_to_cstr(MemoryType type) {
    return i_type_to_cstr[type];
}
