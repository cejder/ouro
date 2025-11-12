#pragma once

#include "common.hpp"
#include "scene_constants.hpp"

#include <tinycthread.h>

// Need for tinycthread on macOS
#ifdef call_once
#undef call_once
#endif

fwd_decl(ATerrain);

#define ENTITY_SPAWN_COMMAND_QUEUE_MAX 16384

// Command queue for thread-safe entity spawning
enum EntitySpawnCommandType : U8 {
    ENTITY_SPAWN_CMD_RANDOM_VEGETATION,
    ENTITY_SPAWN_CMD_COUNT
};

struct EntitySpawnCommand {
    EntitySpawnCommandType type;
    ATerrain *terrain;
    SZ count;
    BOOL notify;
};

struct EntitySpawnCommandQueue {
    EntitySpawnCommand commands[ENTITY_SPAWN_COMMAND_QUEUE_MAX];
    U32 count;
    mtx_t mutex;
};

struct EntityTestOverworldSet {
    EID cesiums[CESIUM_COUNT];
    EID lumberyards[LUMBERYARD_COUNT];
};

void entity_spawn_random_vegetation_on_terrain(ATerrain *terrain, SZ count, BOOL notify);
void entity_despawn_random_vegetation(SZ count, BOOL notify);
void entity_cb_spawn_random_vegetation_on_terrain(void *data);
void entity_cb_despawn_random_vegetation(void *data);

void entity_spawn_npc(SZ count, BOOL notify);
void entity_despawn_npc(SZ count, BOOL notify);
void entity_cb_spawn_npc(void *data);
void entity_cb_despawn_npc(void *data);

void entity_spawn_test_overworld_set(EntityTestOverworldSet *set);
void entity_init_test_overworld_set_talkers(EntityTestOverworldSet *set, void (*cb_trigger_gong)(void *data), void (*cb_trigger_end)(void *data));

// Thread-safe command queue API (safe to call from worker threads)
void entity_spawn_queue_random_vegetation_on_terrain(ATerrain *terrain, SZ count, BOOL notify);

// Main thread only: process all queued commands
void entity_spawn_process_command_queue();
