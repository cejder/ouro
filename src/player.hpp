#pragma once

#include "common.hpp"
#include "ease.hpp"
#include "entity.hpp"
#include "math.hpp"
#include "scene.hpp"

#define PLAYER_RADIUS 2.0F
#define PLAYER_HEIGHT_TO_EYES 2.0F
#define PLAYER_REACTION_DISTANCE 4.0F
#define PLAYER_MOVE_DISTANCE 4.0F
#define PLAYER_MOVE_DURATION 0.35F
#define PLAYER_TURN_DURATION 0.30F
#define PLAYER_MOVE_SPEED 10.0F
#define PLAYER_MOVE_EASE_TYPE EASE_IN_OUT_QUAD
#define PLAYER_TURN_EASE_TYPE EASE_IN_OUT_QUAD

fwd_decl(AModel);
fwd_decl(AFont);
fwd_decl(ATexture);
fwd_decl(World);

struct PlayerTurnState {
    F32 start_yaw;
    F32 target_yaw;
    F32 current_time;
    F32 duration;
    BOOL is_turning;
    EaseType ease_type;
};

struct PlayerMoveState {
    F32 start_pos_x;
    F32 start_pos_z;
    F32 start_tgt_x;
    F32 start_tgt_z;
    F32 target_pos_x;
    F32 target_pos_z;
    F32 target_tgt_x;
    F32 target_tgt_z;
    F32 current_time;
    F32 duration;
    BOOL is_moving;
    EaseType ease_type;
};

struct PlayerKeepOnGroundState {
    F32 ground_pos;
    F32 fall_velocity;
    F32 last_height;
};


struct Player {
    PlayerTurnState turn_state;
    PlayerMoveState move_state;
    PlayerKeepOnGroundState keep_on_ground_state;

    BOOL non_player_camera;
    BOOL on_triangle_floor;
    Camera3D cameras[SCENE_COUNT];
    Vector3 previous_position;  // For swept collision detection
    OrientedBoundingBox obb;
    BOOL in_frustum;

    ATexture *debug_icon;
};

Player extern g_player;

void player_init();
void player_update(F32 dt, F32 dtu);
void player_input_update(F32 dt, F32 dtu);
void player_set_camera(SceneType type);
void player_draw_3d_hud();
void player_draw_3d_dbg();
void player_step();
void player_dump_cameras();
