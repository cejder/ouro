#include "entity_spawn.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "entity.hpp"
#include "entity_actor.hpp"
#include "message.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "string.hpp"
#include "word.hpp"
#include "world.hpp"

#include <raymath.h>
#include <glm/gtc/type_ptr.hpp>

void entity_spawn_random_vegetation_on_terrain(ATerrain *terrain, SZ count, BOOL notify) {
    if (g_world->active_ent_count >= WORLD_MAX_ENTITIES) {
        mwod("Could not spawn entity, world is full.", ORANGE, 5.0F);
        return;
    }

    F32 constexpr max_slope              = 0.5F;                        // Maximum allowed slope for vegetation placement
    F32 constexpr min_spacing            = 5.0F;                        // Minimum distance between things
    SZ constexpr max_attempts            = (SZ)WORLD_MAX_ENTITIES * 2;  // Reduced since grid makes this more efficient
    F32 constexpr bush_bias              = 0.33F;
    S32 constexpr tree_variant           = 5;
    S32 constexpr bush_variant           = 3;
    S32 constexpr added_padding          = 100;
    S32 constexpr tree_padded            = tree_variant * added_padding;
    S32 constexpr bush_padded            = bush_variant * added_padding;
    S32 constexpr possibilities          = (S32)((F32)(tree_padded) + ((F32)(bush_padded)*bush_bias));
    SZ constexpr max_vegetation_per_cell = 8;  // Limit vegetation density per cell

    SZ attempts = 0;

    do {
        // Pick a random cell instead of a random world position
        SZ const cell_index = (SZ)random_s32(0, GRID_TOTAL_CELLS - 1);
        GridCell *cell      = grid_get_cell_by_index(cell_index);
        if (!cell) { continue; }

        // Count existing vegetation in this cell
        SZ vegetation_count = 0;
        for (SZ i = 0; i < cell->entity_count; ++i) {
            EID const entity_id = cell->entities[i];
            if (g_world->type[entity_id] == ENTITY_TYPE_VEGETATION) { vegetation_count++; }
        }

        // Skip cells that already have too much vegetation
        if (vegetation_count >= max_vegetation_per_cell) {
            attempts++;
            continue;
        }

        // Get the cell center and add some randomness within the cell
        Vector3 const cell_center = grid_cell_center(cell_index);
        F32 const cell_size       = g_grid.cell_size;
        F32 const half_cell       = cell_size * 0.5F;

        Vector3 spawn_point = {
            cell_center.x + random_f32(-half_cell * 0.8F, half_cell * 0.8F),
            0.0F,
            cell_center.z + random_f32(-half_cell * 0.8F, half_cell * 0.8F),
        };

        // Ensure the spawn point is within terrain bounds
        if (!grid_is_position_valid(spawn_point)) {
            attempts++;
            continue;
        }

        // Check terrain slope at this position
        F32 const center_height  = math_get_terrain_height(terrain, spawn_point.x, spawn_point.z);
        F32 const right_height   = math_get_terrain_height(terrain, spawn_point.x + 1.0F, spawn_point.z);
        F32 const forward_height = math_get_terrain_height(terrain, spawn_point.x, spawn_point.z + 1.0F);

        // Calculate slope
        F32 const dx    = right_height - center_height;
        F32 const dz    = forward_height - center_height;
        F32 const slope = math_sqrt_f32((dx * dx) + (dz * dz));

        if (slope > max_slope) {
            attempts++;
            continue;
        }

        spawn_point.y = center_height;

        // Check spacing using grid query for nearby entities
        EID nearby_entities[GRID_NEARBY_ENTITIES_MAX];  // Buffer for nearby entities
        SZ nearby_count = 0;
        grid_query_entities_in_radius(spawn_point, min_spacing, nearby_entities, &nearby_count, GRID_NEARBY_ENTITIES_MAX);

        BOOL too_close = false;
        for (SZ i = 0; i < nearby_count; ++i) {
            EID const nearby_id = nearby_entities[i];
            F32 const distance = Vector3Distance(spawn_point, g_world->position[nearby_id]);
            if (distance < min_spacing) {
                too_close = true;
                break;
            }
        }

        if (too_close) {
            attempts++;
            continue;
        }

        // Select vegetation type
        S32 rnd_idx              = random_s32(0, possibilities - 1);
        String const *model_name = nullptr;
        String const *name       = nullptr;
        F32 base_scale           = 1.0F;
        F32 max_scale_add        = 0.0F;
        Color tint               = color_variation(WHITE, 50);

        if (rnd_idx >= 0 && rnd_idx < tree_padded) {
            rnd_idx      %= tree_variant;
            name          = TS("Tree_Type_%d", rnd_idx);
            base_scale   += 3.0F;
            max_scale_add = 0.5F;
            model_name    = TS("tree_%d.glb", rnd_idx);
        } else {
            // rnd_idx      -= tree_variant;
            // rnd_idx      %= bush_variant;
            // name          = TS("Bush_Type_%d", rnd_idx);
            // base_scale   += 0.5F;
            // max_scale_add = 1.0F;
            // model_name    = TS("bush_%d.glb", rnd_idx);
            // TODO: OVERWRITING
            rnd_idx      %= tree_variant;
            name          = TS("Tree_Type_%d", rnd_idx);
            base_scale   += 3.0F;
            max_scale_add = 0.5F;
            model_name    = TS("tree_%d.glb", rnd_idx);
        }

        spawn_point.y += 0.085F;  // Slight offset above ground

        F32 const third_max_scale = max_scale_add / 3.0F;
        Vector3 const scale = {
            base_scale + random_f32(0.0F, third_max_scale),
            base_scale + random_f32(0.0F, max_scale_add),
            base_scale + random_f32(0.0F, third_max_scale),
        };

        EID const new_entity = entity_create(ENTITY_TYPE_VEGETATION, name->c, spawn_point, random_f32(0.0F, 360.0F), scale, tint, model_name->c);
        if (new_entity != INVALID_EID) {
            count--;

            if (notify) { mio(TS("Spawned vegetation at \\ouc{#ffff00ff}%.2f, %.2f, %.2f (cell %zu)", spawn_point.x, spawn_point.y, spawn_point.z, cell_index)->c, WHITE); }
        }

        attempts++;

        if (attempts >= max_attempts) {
            mw(TS("Canceled vegetation spawning, too many attempts, we could not spawn %zu vegetation after %zu attempts.", count, attempts)->c, ORANGE);
            break;
        }
    } while (count > 0);
}

void entity_despawn_random_vegetation(SZ count, BOOL notify) {
    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (g_world->type[i] != ENTITY_TYPE_VEGETATION) { continue; }

        Vector3 const position = g_world->position[i];
        entity_destroy(i);

        if (notify) { mio(TS("Despawned vegetation at \\ouc{#ffff00ff}%.2f, %.2f, %.2f", position.x, position.y, position.z)->c, WHITE); }

        count--;
        if (count == 0) { break; }
    }
}

void entity_cb_spawn_random_vegetation_on_terrain(void *data) {
    auto *terrain = (ATerrain *)data;
    entity_spawn_random_vegetation_on_terrain(terrain, 1, false);
}

void entity_cb_despawn_random_vegetation(void *data) {
    unused(data);

    entity_despawn_random_vegetation(1, false);
}

void entity_spawn_random_npc_around_camera(SZ count, BOOL notify) {
    Camera3D const camera = c3d_get();

    for (SZ i = 0; i < count; ++i) {
        if (g_world->active_ent_count >= WORLD_MAX_ENTITIES) {
            mwod("Could not spawn entity, world is full.", ORANGE, 5.0F);
            return;
        }
        // Generate random point in a filled sphere
        F32 const radius = random_f32(0.0F, 50.0F);                 // Random radius for filled sphere
        F32 const theta  = random_f32(0.0F, 2.0F * glm::pi<F32>()); // Azimuthal angle
        F32 const phi    = math_acos_f32(random_f32(-1.0F, 1.0F));  // Polar angle (for 3D distribution)

        Vector3 const position = {
            (math_sin_f32(phi) * math_cos_f32(theta) * radius) + camera.position.x,
            (math_sin_f32(phi) * math_sin_f32(theta) * radius) + camera.position.y,
            (math_cos_f32(phi) * radius) + camera.position.z,
        };

        Color const tint      = color_random_vibrant();
        F32 const rotation    = random_f32(0.0F, 360.0F);
        String *name          = nullptr;
        C8 const *model_name  = nullptr;
        EntityType const type = ENTITY_TYPE_NPC;
        Vector3 scale         = {1.0F, 1.0F, 1.0F};

        switch (random_s32(0, 0)) {  // NOTE: Only spawning cesium now
            case 0: {
                name                 = word_generate_name();
                model_name           = "greenman.glb";
                F32 const rand_scale = random_f32(CESIUM_MIN_SCALE, CESIUM_MAX_SCALE);
                scale                = {rand_scale, rand_scale, rand_scale};
            } break;
            case 1: {
                name       = TS("Seagull %u", g_world->active_ent_count);
                model_name = "seagull.m3d";
                scale      = {0.05F, 0.05F, 0.05F};
            } break;
            case 2: {
                name       = TS("Female Survivor %u", g_world->active_ent_count);
                model_name = "female_survivor_0.glb";
                scale      = {0.25F, 0.25F, 0.25F};
            } break;
            case 3: {
                name       = TS("Female Survivor %u", g_world->active_ent_count);
                model_name = "female_survivor_1.glb";
                scale      = {0.25F, 0.25F, 0.25F};
            } break;
            case 4: {
                name       = TS("Male Survivor %u", g_world->active_ent_count);
                model_name = "male_survivor_0.glb";
                scale      = {0.25F, 0.25F, 0.25F};
            } break;
            case 5: {
                name       = TS("Male Survivor %u", g_world->active_ent_count);
                model_name = "male_survivor_1.glb";
                scale      = {0.25F, 0.25F, 0.25F};
            } break;
            default: {
                _unreachable_();
            }
        }

        EID const id = entity_create(type, name->c, position, rotation, scale, tint, model_name);
        if (id == INVALID_EID) {
            mw("Could not create NPC, entity count is at maximum.", ORANGE);
            return;
        }

        entity_enable_actor(id);
        entity_actor_start_looking_for_target(id, ENTITY_TYPE_VEGETATION);

        if (notify) { mio(TS("Created NPC \\ouc{%s}%s", "#00ffffff", name->c)->c, WHITE); }
    }
}

void entity_despawn_random_npc_around_camera(SZ count, BOOL notify) {
    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (g_world->type[i] != ENTITY_TYPE_NPC) { continue; }

        if (notify) { mio(TS("Removed NPC \\ouc{%s}%s", "#00ffffff", g_world->name[i])->c, WHITE); }

        entity_destroy(i);

        count--;
        if (count == 0) { break; }
    }
}

void entity_cb_spawn_random_npc_around_camera(void *data) {
    unused(data);

    entity_spawn_random_npc_around_camera(1, true);
}

void entity_cb_despawn_random_npc_around_camera(void *data) {
    unused(data);

    entity_despawn_random_npc_around_camera(1, true);
}

void entity_spawn_test_overworld_set(EntityTestOverworldSet *set) {
    Vector3 position = CESIUM_POSITION;
    Vector3 scale    = {};

    for (EID &cesium : set->cesiums) {
        F32 const base_spacing = 1.0F;
        F32 const radius       = base_spacing * math_sqrt_f32(CESIUM_COUNT);

        F32 const angle      = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const distance   = math_sqrt_f32(random_f32(0.0F, 1.0F)) * radius;
        Vector3 const offset = {math_cos_f32(angle) * distance, 0.0F, math_sin_f32(angle) * distance};

        Vector3 spawn_position = {position.x + offset.x, position.y, position.z + offset.z};
        spawn_position.y       = math_get_terrain_height(g_world->base_terrain, spawn_position.x, spawn_position.z);

        F32 const rand_scale = random_f32(CESIUM_MIN_SCALE, CESIUM_MAX_SCALE);
        Color const tint     = color_random_vibrant();
        scale                = {rand_scale, rand_scale, rand_scale};
        cesium               = entity_create(ENTITY_TYPE_NPC, "", spawn_position, 0.0F, scale, tint, "greenman.glb");

        entity_enable_actor(cesium);
        entity_actor_start_looking_for_target(cesium, ENTITY_TYPE_VEGETATION);
    }

    if (LUMBERYARD_COUNT == 0) { return; }

    // Spawn lumberyards in a grid pattern
    // Calculate grid dimensions: try to make it roughly square
    SZ const grid_cols = (SZ)math_sqrt_f32((F32)LUMBERYARD_COUNT);
    SZ const grid_rows = (LUMBERYARD_COUNT + grid_cols - 1) / grid_cols; // Ceiling division

    // Grid spacing and starting position
    F32 const spacing_x = 100.0F; // Distance between lumberyards in X
    F32 const spacing_z = 100.0F; // Distance between lumberyards in Z
    F32 const start_x = (F32)A_TERRAIN_DEFAULT_SIZE / 3;
    F32 const start_z = (F32)A_TERRAIN_DEFAULT_SIZE / 3;
    F32 const rand_scale = random_f32(4.0F, 6.0F);

    SZ lumberyard_index = 0;
    for (SZ row = 0; row < grid_rows && lumberyard_index < LUMBERYARD_COUNT; ++row) {
        for (SZ col = 0; col < grid_cols && lumberyard_index < LUMBERYARD_COUNT; ++col) {
            position.x = start_x + ((F32)col * spacing_x);
            position.z = start_z + ((F32)row * spacing_z);
            position.y = math_get_terrain_height(g_world->base_terrain, position.x, position.z);
            position.y -= 5.0F; // Start a bit lower to hide our shame (mesh not aligning with floor bla)

            F32 const rotation = random_f32(0.0F, 360.0F);
            set->lumberyards[lumberyard_index] = entity_create(ENTITY_TYPE_BUILDING_LUMBERYARD, "LUMBERYARD", position, rotation, {
                rand_scale,
                rand_scale * 10, // Higher than wide
                rand_scale,
            }, BROWN, "fountain-round.glb");
            lumberyard_index++;
        }
    }
}

void entity_init_test_overworld_set_talkers(EntityTestOverworldSet *set, void (*cb_trigger_gong)(void *data), void (*cb_trigger_end)(void *data)) {
#if OURO_TALK
#include T_LOAD_DIALOGUE(cesium0)
    T_SET(cesium0, set->cesiums[0]);

#include T_LOAD_DIALOGUE(cesium1)
    T_SET(cesium1, set->cesiums[1]);

#include T_LOAD_DIALOGUE(cesium2)
    T_SET(cesium2, set->cesiums[2]);

#include T_LOAD_DIALOGUE(cesium3)
    T_SET(cesium3, set->cesiums[3]);

#include T_LOAD_DIALOGUE(cesium4)
    T_SET(cesium4, set->cesiums[4]);

#include T_LOAD_DIALOGUE(cesium5)
    T_SET(cesium5, set->cesiums[5]);

#include T_LOAD_DIALOGUE(cesium6)
    T_SET(cesium6, set->cesiums[6]);

#include T_LOAD_DIALOGUE(cesium7)
    T_SET(cesium7, set->cesiums[7]);

#include T_LOAD_DIALOGUE(cesium8)
    T_SET(cesium8, set->cesiums[8]);

#include T_LOAD_DIALOGUE(cesium9)
    T_SET(cesium9, set->cesiums[9]);

#include T_LOAD_DIALOGUE(cesium10)
    T_SET(cesium10, set->cesiums[10]);

#include T_LOAD_DIALOGUE(cesium11)
    T_SET(cesium11, set->cesiums[11]);

#include T_LOAD_DIALOGUE(cesium12)
    T_SET(cesium12, set->cesiums[12]);

#include T_LOAD_DIALOGUE(cesium13)
    T_SET(cesium13, set->cesiums[13]);

#include T_LOAD_DIALOGUE(cesium14)
    T_SET(cesium14, set->cesiums[14]);

#include T_LOAD_DIALOGUE(cesium15)
    T_SET(cesium15, set->cesiums[15]);

#include T_LOAD_DIALOGUE(cesium16)
    T_SET(cesium16, set->cesiums[16]);

#include T_LOAD_DIALOGUE(cesium17)
    T_SET(cesium17, set->cesiums[17]);

#include T_LOAD_DIALOGUE(cesium18)
    T_SET(cesium18, set->cesiums[18]);

#include T_LOAD_DIALOGUE(cesium19)
    T_SET(cesium19, set->cesiums[19]);

#include T_LOAD_DIALOGUE(cesium20)
    T_SET(cesium20, set->cesiums[20]);

#include T_LOAD_DIALOGUE(cesium21)
    T_SET(cesium21, set->cesiums[21]);

#include T_LOAD_DIALOGUE(cesium22)
    T_SET(cesium22, set->cesiums[22]);

#include T_LOAD_DIALOGUE(cesium23)
    T_SET(cesium23, set->cesiums[23]);

#include T_LOAD_DIALOGUE(cesium24)
    T_SET(cesium24, set->cesiums[24]);

// #include T_LOAD_DIALOGUE(seagull)
//     T_SET(seagull, set->seagull);
#endif
}
