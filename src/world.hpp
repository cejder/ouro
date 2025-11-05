#pragma once

#include "asset.hpp"
#include "common.hpp"
#include "entity.hpp"
#include "entity_actor.hpp"
#include "entity_building.hpp"
#include "grid.hpp"
#include "math.hpp"
#include "player.hpp"
#include "talk.hpp"

#include <raylib.h>

#define LIGHTS_MAX 10
#define LIGHT_DEFAULT_INTENSITY 2000.0F

#define WORLD_MAX_ENTITIES 5000

fwd_decl(ATerrain);
fwd_decl(ASound);
fwd_decl(AModel);
fwd_decl(AShader);

// Light Types:
// - POINT: Emits light omnidirectionally from a position (like a light bulb)
// - SPOT: Cone of light from a position in a direction (like a flashlight)
enum LightType : U8 {
    LIGHT_TYPE_POINT = 0,
    LIGHT_TYPE_SPOT = 1,
};

struct Light {
    BOOL dirty;
    BOOL in_frustum;

    S32 enabled;
    LightType type;
    Vector3 position;
    Vector3 direction;
    F32 color[4];
    F32 intensity;
    F32 inner_cutoff;
    F32 outer_cutoff;

    S32 enabled_loc;
    S32 type_loc;
    S32 position_loc;
    S32 direction_loc;
    S32 intensity_loc;
    S32 color_loc;
    S32 inner_cutoff_loc;
    S32 outer_cutoff_loc;
};

struct Lighting {
    BOOL initialized;
    Light lights[LIGHTS_MAX];
    AShader *model_shader;
    S32 model_shader_view_pos_loc;
    S32 model_shader_ambient_color_loc;
};

struct Fog {
    AShader *shader;
    F32 density;
    Color color;
    S32 density_loc;
    S32 color_loc;
};

// TODO: This needs to be skinnier

// TODO: MOVE THIS
struct TriangleMeshCollider {
    AModel *model;
    Vector3 offset;
};

struct World {
    // Lighting and fog (scene-specific)
    Lighting lighting;
    Fog fog;

    SZ visible_vertex_count;
    ATerrain *base_terrain;
    Player player;
    EID selected_id;
    U32 active_ent_count;
    U32 entity_type_counts[ENTITY_TYPE_COUNT];
    U32 max_gen;
    BOOL pawn_interactions_dirty;

    // Active entity optimization: array of active entity IDs for fast iteration
    EID active_entities[WORLD_MAX_ENTITIES];
    SZ active_entity_count;

    // NOTE: Field order optimized for cache locality - hot data first

    alignas(32) U32 flags[WORLD_MAX_ENTITIES];
    alignas(32) U32 generation[WORLD_MAX_ENTITIES];
    alignas(32) EntityType type[WORLD_MAX_ENTITIES];
    alignas(32) F32 lifetime[WORLD_MAX_ENTITIES];

    alignas(32) Vector3 position[WORLD_MAX_ENTITIES];
    alignas(32) F32 rotation[WORLD_MAX_ENTITIES];
    alignas(32) Vector3 scale[WORLD_MAX_ENTITIES];
    alignas(32) OrientedBoundingBox obb[WORLD_MAX_ENTITIES];
    alignas(32) F32 radius[WORLD_MAX_ENTITIES];

    alignas(32) EntityActor actor[WORLD_MAX_ENTITIES];

    alignas(32) C8 model_name[WORLD_MAX_ENTITIES][A_NAME_MAX_LENGTH];
    alignas(32) Color tint[WORLD_MAX_ENTITIES];

    alignas(32) EntityHealth health[WORLD_MAX_ENTITIES];
    alignas(32) C8 name[WORLD_MAX_ENTITIES][ENTITY_NAME_MAX_LENGTH];

    alignas(32) EntityTalker talker[WORLD_MAX_ENTITIES];
    alignas(32) EntityBuilding building[WORLD_MAX_ENTITIES];
    alignas(32) Vector3 original_scale[WORLD_MAX_ENTITIES];

    alignas(32) TriangleMeshCollider triangle_collider[WORLD_MAX_ENTITIES];

    alignas(32) EntityAnimation animation[WORLD_MAX_ENTITIES];

    struct {
        U8 follower_counts[WORLD_MAX_ENTITIES];
        BOOL dirty;
    } follower_cache;

    struct {
        EID actors[32];
        U8 count;
    } target_trackers[WORLD_MAX_ENTITIES];
};

// World state - scenes set g_world to their local world
World extern *g_world;

void world_init(World *world);
void world_reset(World *world);
void world_update(F32 dt, F32 dtu);
void world_draw_2d();
void world_draw_2d_hud();
void world_draw_2d_dbg();
void world_draw_3d();
void world_draw_3d_sketch();
void world_draw_3d_hud();
void world_draw_3d_dbg();
void world_set_selected_entity(EID id);
void world_vegetation_collision();
BOOL world_triangle_mesh_collision();
void world_set_entity_triangle_collision(EID id, AModel *model, Vector3 offset);
F32 world_get_distance_to_player(Vector3 position);
void world_randomly_rotate_entities();
void world_dump_all_entities();
void world_dump_all_entities_cb(void *data);

// Lighting functions (operate on g_world->lighting)
void lighting_init();
void lighting_disable_all_lights();
void lighting_set_point_light(SZ idx, BOOL enabled, Vector3 position, Color color, F32 intensity);
void lighting_set_spot_light(SZ idx, BOOL enabled, Vector3 position, Vector3 direction, Color color, F32 intensity, F32 inner_cutoff, F32 outer_cutoff);
void lighting_set_light_enabled(SZ idx, BOOL enabled);
void lighting_set_light_position(SZ idx, Vector3 position);
void lighting_default_lights_setup();
void lighting_update(Camera3D *camera);
void lighting_draw_2d_dbg();
void lighting_draw_3d_dbg();
void lighting_dump();

// Fog functions (operate on g_world->fog)
void fog_init(AShader *shader);
void fog_update();

struct WorldRecorder {
    BOOL record_world_state;
    SZ record_world_state_cursor;
    BOOL world_state_go_back;
    BOOL world_state_go_forward;
    SZ world_state_cursor;
    BOOL keep_looping;
};

void world_recorder_init();
void world_recorder_draw_2d_hud();
void world_recorder_update();
void world_recorder_toggle_record_state();
void world_recorder_toggle_loop_state();
void world_recorder_backward_state();
void world_recorder_forward_state();
void world_recorder_clear();
void world_recorder_delete_recorded_state();
BOOL world_recorder_is_recording_state();
BOOL world_recorder_is_looping_state();
SZ world_recorder_get_record_cursor();
SZ world_recorder_get_playback_cursor();
SZ *world_recorder_get_playback_cursor_ptr();
void world_recorder_save_state_to_file(C8 const *file_path);
void world_recorder_load_state_to_file(C8 const *file_path);
SZ world_recorder_get_state_size_bytes();
SZ world_recorder_get_total_recorded_size_bytes();
SZ world_recorder_get_actual_disk_usage_bytes();

void world_update_follower_cache();
S32 world_followed_by_count(EID id);
void world_mark_follower_cache_dirty();
void world_target_tracker_add(EID target_id, EID actor_id);
void world_target_tracker_remove(EID target_id, EID actor_id);
void world_notify_actors_target_destroyed(EID destroyed_target_id);
