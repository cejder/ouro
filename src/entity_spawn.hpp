#pragma once

#include "common.hpp"
#include "scene_constants.hpp"
#include "entity.hpp"
#include "asset.hpp"

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
    ENTITY_SPAWN_CMD_ARBITRARY_ENTITY,
    ENTITY_SPAWN_CMD_COUNT
};

struct EntitySpawnCommand {
    EntitySpawnCommandType type;
    union {
        // For ENTITY_SPAWN_CMD_RANDOM_VEGETATION
        struct {
            ATerrain *terrain;
            SZ count;
            BOOL notify;
        } vegetation;

        // For ENTITY_SPAWN_CMD_ARBITRARY_ENTITY
        struct {
            EntityType entity_type;
            C8 name[ENTITY_NAME_MAX_LENGTH];
            Vector3 position;
            F32 rotation;
            Vector3 scale;
            Color tint;
            C8 model_name[A_PATH_MAX_LENGTH];
        } arbitrary;
    };
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
void entity_spawn_queue_arbitrary_entity(EntityType type, C8 const *name, Vector3 position, F32 rotation, Vector3 scale, Color tint, C8 const *model_name);

// Main thread only: process all queued commands
void entity_spawn_process_command_queue();
