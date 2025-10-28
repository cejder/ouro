#pragma once

#include "common.hpp"
#include "scene_constants.hpp"

fwd_decl(ATerrain);

struct EntityTestOverworldSet {
    EID cesiums[CESIUM_COUNT];
    EID lumberyards[LUMBERYARD_COUNT];
};

void entity_spawn_random_vegetation_on_terrain(ATerrain *terrain, SZ count, BOOL notify);
void entity_despawn_random_vegetation(SZ count, BOOL notify);
void entity_cb_spawn_random_vegetation_on_terrain(void *data);
void entity_cb_despawn_random_vegetation(void *data);

void entity_spawn_random_npc_around_camera(SZ count, BOOL notify);
void entity_despawn_random_npc_around_camera(SZ count, BOOL notify);
void entity_cb_spawn_random_npc_around_camera(void *data);
void entity_cb_despawn_random_npc_around_camera(void *data);

void entity_spawn_test_overworld_set(EntityTestOverworldSet *set);
void entity_init_test_overworld_set_talkers(EntityTestOverworldSet *set, void (*cb_trigger_gong)(void *data), void (*cb_trigger_end)(void *data));
