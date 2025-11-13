#pragma once

#include "common.hpp"
#include "math.hpp"
#include "talk.hpp"

#define ENTITY_NAME_MAX_LENGTH 32

fwd_decl(World);
fwd_decl(String);

#define INVALID_EID ((EID)U32_MAX)

enum EntityType : U8 {
    ENTITY_TYPE_NONE,
    ENTITY_TYPE_NPC,
    ENTITY_TYPE_VEGETATION,
    ENTITY_TYPE_BUILDING_LUMBERYARD,
    ENTITY_TYPE_PROP,
    ENTITY_TYPE_COUNT,
};

enum EntityFlagBits : U8 {
    ENTITY_FLAG_IN_USE,
    ENTITY_FLAG_IN_FRUSTUM,
    ENTITY_FLAG_TALKER,
    ENTITY_FLAG_ACTOR,
    ENTITY_FLAG_COLLIDING_PLAYER,
    ENTITY_FLAG_TRIANGLE_COLLISION,
    ENTITY_FLAG_COUNT,
};

#define ENTITY_FLAG_MASK(flag) (1U << (flag))
#define ENTITY_HAS_FLAG(flags, flag) FLAG_HAS(flags, ENTITY_FLAG_MASK(flag))
#define ENTITY_SET_FLAG(flags, flag) FLAG_SET(flags, ENTITY_FLAG_MASK(flag))
#define ENTITY_CLEAR_FLAG(flags, flag) FLAG_CLEAR(flags, ENTITY_FLAG_MASK(flag))

struct EntityTalker {
    Conversation conversation;
    BOOL in_reach;
    BOOL is_enabled;
    BOOL is_active;
    F32 cursor_timer;
};

struct EntityHealth {
    S32 max;
    S32 current;
};

#define ENTITY_MAX_BONES 128

struct EntityAnimation {
    BOOL has_animations;
    S32 bone_count;
    F32 anim_fps;
    U32 anim_index;
    U32 anim_frame;
    F32 anim_time;
    F32 anim_speed;
    BOOL anim_loop;
    BOOL anim_playing;
    BOOL is_blending;
    F32 blend_time;
    F32 blend_duration;
    U32 prev_anim_index;
    U32 prev_anim_frame;
};

// Computed bone matrices - stored separately from World (not saved by recorder)
struct AnimationBoneData {
    Matrix bone_matrices[ENTITY_MAX_BONES];
    Matrix prev_bone_matrices[ENTITY_MAX_BONES];
};

void entity_init();
EID entity_create(EntityType type, C8 const *name, Vector3 position, F32 rotation, Vector3 scale, Color tint, C8 const *model_name);
void entity_destroy(EID id);
C8 const *entity_type_to_cstr(EntityType type);
Color entity_type_to_color(EntityType type);
BOOL entity_is_valid(EID id);
EID entity_find_at_mouse();
EID entity_find_at_mouse_with_type(EntityType type);
EID entity_find_at_screen_point(Vector2 screen_point);
EID entity_find_at_screen_point_with_type(Vector2 screen_point, EntityType type);
void entity_enable_talker(EID id, DialogueRoot *root, SZ nodes_count);
void entity_disable_talker(EID id);
void entity_enable_actor(EID id);
void entity_disable_actor(EID id);
BOOL entity_collides_sphere(EID id, Vector3 center, F32 radius);
BOOL entity_collides_sphere_outside(EID id, Vector3 center, F32 radius);
BOOL entity_collides_other(EID id, EID other_id);
BOOL entity_collides_player(EID id);
F32 entity_distance_to_player(EID id);
void entity_move(EID id, MoveDirection direction, F32 length);
void entity_move_target(EID id, Vector3 target, F32 length);
void entity_add_position(EID id, Vector3 position);
void entity_set_position(EID id, Vector3 position);
Vector2 entity_get_position_vec2(EID id);
void entity_add_rotation(EID id, F32 rotation);
void entity_set_rotation(EID id, F32 rotation);
void entity_face_target(EID id, EID target_id, F32 turn_speed, F32 dt);
void entity_add_scale(EID id, Vector3 scale);
void entity_set_scale(EID id, Vector3 scale);
void entity_set_model(EID id, C8 const *model_name);
void entity_set_animation(EID id, U32 anim_index, BOOL loop, F32 speed);
void entity_play_animation(EID id);
void entity_pause_animation(EID id);
void entity_stop_animation(EID id);
void entity_set_animation_frame(EID id, U32 frame);
BOOL entity_damage(EID id, S32 damage);
EID entity_find_by_name(C8 const *name);
F32 entity_distance_to_position(EID id, Vector2 position);
F32 entity_distance_to_entity(EID id, EID target_id);
Vector2 entity_get_approach_position_from(EID target_id, Vector2 from_position);
