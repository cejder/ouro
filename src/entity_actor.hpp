#pragma once

#include "common.hpp"
#include "ease.hpp"

#include <raylib.h>

fwd_decl(World);
fwd_decl_enum(EntityType, U8);

#define ACTION_DURATION_DELIVERY 1.0F
#define ACTION_DURATION_HARVEST 3.0F
#define ACTOR_WOOD_COLLECTED_MAX 3
#define ACTOR_MAX_STUCK_TIME 2.0F
#define ATTACK_DAMAGE 25.0F
#define ATTACK_RANGE 3.0F
#define ATTACK_RANGE_EXIT 6.0F
#define HARVEST_TARGET_MAX_FOLLOWERS 3
#define MAX_ACTORS_PER_TARGET 32

// Harvest impact effect (spawned once when starting harvest)
#define HARVEST_IMPACT_PARTICLE_COUNT 25
#define HARVEST_IMPACT_SIZE_MULTIPLIER 1.5F

// Harvest active effect (spawned continuously during harvest)
#define HARVEST_ACTIVE_PARTICLES_PER_SECOND 30.0F
#define HARVEST_ACTIVE_PARTICLE_COUNT 1
#define HARVEST_ACTIVE_SIZE_MULTIPLIER 0.4F

// Harvest complete effect (spawned once when harvest finishes)
#define HARVEST_COMPLETE_PARTICLE_COUNT 20
#define HARVEST_COMPLETE_SIZE_MULTIPLIER 0.8F

enum EntityMovementState : U8 {
    ENTITY_MOVEMENT_STATE_IDLE,
    ENTITY_MOVEMENT_STATE_MOVING,
    ENTITY_MOVEMENT_STATE_TURNING,
    ENTITY_MOVEMENT_STATE_STOPPING,
    ENTITY_MOVEMENT_STATE_COUNT
};

enum EntityMovementGoal : U8 {
    ENTITY_MOVEMENT_GOAL_NONE,
    ENTITY_MOVEMENT_GOAL_MOVE_TO_POSITION,
    ENTITY_MOVEMENT_GOAL_TURN_TO_ANGLE,
    ENTITY_MOVEMENT_GOAL_STOP,
    ENTITY_MOVEMENT_GOAL_COUNT
};

enum EntityBehaviorState : U8 {
    ENTITY_BEHAVIOR_STATE_IDLE,
    ENTITY_BEHAVIOR_STATE_GOING_TO_TARGET,
    ENTITY_BEHAVIOR_STATE_HARVESTING_TARGET,
    ENTITY_BEHAVIOR_STATE_DELIVERING_TO_LUMBERYARD,
    ENTITY_BEHAVIOR_STATE_ATTACKING_NPC,
    ENTITY_BEHAVIOR_STATE_COUNT
};

struct EntityMovementController {
    EntityMovementState state;

    F32 speed;
    F32 current_speed;
    F32 acceleration;
    Vector3 velocity;

    Vector3 separation_force;
    Vector3 last_position;
    F32 stuck_timer;
    F32 target_offset_angle;
    BOOL use_target_offset;

    EntityMovementGoal goal_type;
    Vector3 target_position;
    F32 target_angle;
    EID target_id;
    U32 target_gen;

    BOOL goal_completed;
    BOOL goal_failed;

    F32 turn_start_angle;
    F32 turn_target_angle;
    F32 turn_duration;
    F32 turn_elapsed;
    EaseType turn_ease_type;
};

#define ENTITY_STATE_REASON_MAX 512

struct EntityBehaviorController {
    EntityBehaviorState state;
    C8 state_reason[ENTITY_STATE_REASON_MAX];

    EntityType target_entity_type;
    Vector3 target_position;
    EID target_id;
    U32 target_gen;

    F32 action_timer;
    F32 particle_spawn_timer;  // For frame-rate independent particle spawning

    SZ wood_count;
};

struct EntityActor {
    EntityBehaviorController behavior;
    EntityMovementController movement;
};

void entity_actor_init();
void entity_actor_update(EID id, F32 dt);
void entity_actor_behavior_transition_to_state(EID id, EntityBehaviorState new_state, C8 const *reason);
void entity_actor_search_and_transition_to_target(EID id, C8 const *reason);
void entity_actor_start_looking_for_target(EID id, EntityType target_type);
void entity_actor_set_move_target(EID id, Vector3 target);
void entity_actor_set_attack_target_npc(EID attacker_id, EID target_id);
void entity_actor_clear_target_tracker(EID target_id);
void entity_actor_clear_actor_target(EID actor_id);

C8 const *entity_actor_behavior_state_to_cstr(EntityBehaviorState state);
C8 const *entity_actor_movement_state_to_cstr(EntityMovementState state);
C8 const *entity_actor_movement_goal_to_cstr(EntityMovementGoal goal);

void entity_actor_draw_2d_hud(EID id);
