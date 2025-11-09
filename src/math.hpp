#pragma once

#include "common.hpp"

#include <math.h>
#include <raylib.h>
#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/trigonometric.hpp>

fwd_decl(AFont);
fwd_decl(ATerrain);
fwd_decl(Player);
fwd_decl(World);

#define GRAVITY 50.0F  // TODO: We should change the scale of stuff so we can later change this to 9.81F.

enum MoveDirection : U8 { MOVE_FORWARD, MOVE_BACKWARD, MOVE_LEFT, MOVE_RIGHT, TURN_LEFT, TURN_RIGHT, TURN_180 };

struct OrientedBoundingBox {
    Vector3 center;   // Center point of the box
    Vector3 extents;  // Half-lengths along local axes
    Vector3 axes[3];  // Local axes (right, up, forward)
};

void math_init();
void math_update();
Vector2 measure_text(AFont *font, C8 const *text);
Vector2 measure_text_ouc(AFont *font, C8 const *text);
SZ math_levenshtein_distance(C8 const *a, C8 const *b);
Vector3 math_ray_collision_plane(Ray ray, Vector3 plane_point, Vector3 plane_normal);
Vector3 math_find_lowest_point_in_model(Model *model, Matrix transform);
Vector3 math_find_highest_point_in_model(Model *model, Matrix transform);
BOOL math_is_point_inside_bb(Vector3 point, BoundingBox box);
BOOL math_is_point_inside_obb(Vector3 point, OrientedBoundingBox box);
RayCollision math_ray_collision_obb(Ray ray, OrientedBoundingBox box);
BOOL math_check_collision_obb_sphere(OrientedBoundingBox box, Vector3 center, F32 radius);
BOOL math_check_collision_obbs(OrientedBoundingBox box1, OrientedBoundingBox box2);
Vector3 math_obb_get_bottom_center(OrientedBoundingBox obb);
Vector3 math_obb_get_top_center(OrientedBoundingBox obb);
F32 math_obb_get_height(OrientedBoundingBox obb);
F32 math_obb_get_bottom_y(OrientedBoundingBox obb);
F32 math_obb_get_top_y(OrientedBoundingBox obb);
void math_fill_transform_matrix(Matrix *dst, Vector3 position, Vector3 rotation, Vector3 scale);
RayCollision math_ray_collision_to_terrain(ATerrain *terrain, Vector3 position, Vector3 direction);
Vector3 math_keep_entity_on_ground(ATerrain *terrain, Vector3 position, F32 dt, F32 *fall_velocity, F32 *last_height, F32 *ground_pos, F32 height_offset);
void math_keep_player_on_ground(ATerrain *terrain, F32 dt, Player *player);
F32 math_get_terrain_height(ATerrain const *terrain, F32 world_x, F32 world_z);
Vector3 math_get_terrain_normal(ATerrain const *terrain, F32 world_x, F32 world_z);
F32 math_calculate_y_rotation(F32 pos_x, F32 pos_z, F32 tgt_x, F32 tgt_z);
F32 math_normalize_angle(F32 angle);
F32 math_shortest_angle_distance(F32 start, F32 end);
F32 math_normalize_angle_0_360(F32 angle);
F32 math_normalize_angle_delta(F32 delta);
BOOL math_ray_triangle_intersection(Vector3 ray_origin, Vector3 ray_direction, Vector3 v0, Vector3 v1, Vector3 v2, F32 *out_t);
BOOL math_aabb_sphere_intersection(Vector3 aabb_min, Vector3 aabb_max, Vector3 sphere_center, F32 sphere_radius);
BoundingBox math_transform_aabb(BoundingBox bb, Vector3 position, Vector3 scale);
void math_matrix_decompose(Matrix mat, Vector3 *translation, Quaternion *rotation, Vector3 *scale);
void  math_compute_entity_bone_matrices(EID id);
BOOL math_get_bone_world_position_by_name(EID id, C8 const *bone_name, Vector3 *out_position);
BOOL math_get_bone_world_position_by_index(EID id, S32 bone_index, Vector3 *out_position);

// ===============================================================
// =========================== INLINES ===========================
// ===============================================================

C8 const inline *math_ordinal_suffix(S32 n) {
    if (n % 100 >= 11 && n % 100 <= 13) { return "th"; }
    switch (n % 10) {
        case 1:
            return "st";
        case 2:
            return "nd";
        case 3:
            return "rd";
        default:
            return "th";
    }
}

F32 inline math_round_f32(F32 value) {
    return glm::round(value);
}

F32 inline math_mod_f32(F32 x, F32 y) {
    return glm::mod(x, y);
}

F64 inline math_mod_f64(F64 x, F64 y) {
    return glm::mod(x, y);
}

F32 inline math_abs_f32(F32 value) {
    return glm::abs(value);
}

F64 inline math_abs_f64(F64 value) {
    return glm::abs(value);
}

S32 inline math_abs_s32(S32 value) {
    return glm::abs(value);
}

F32 inline math_copy_sign_f32(F32 x, F32 y) {
    return glm::sign(y) * glm::abs(x);  // GLM equivalent using sign and abs
}

F64 inline math_copy_sign_f64(F64 x, F64 y) {
    return glm::sign(y) * glm::abs(x);  // GLM equivalent using sign and abs
}

F32 inline math_approach_f32(F32 current, F32 target, F32 delta) {
    if (glm::abs(target - current) <= delta) { return target; }
    if (target > current) { return current + delta; }
    return current - delta;
}

F32 inline math_sqrt_f32(F32 value) {
    return glm::sqrt(value);
}

F32 inline math_floor_f32(F32 value) {
    return glm::floor(value);
}

F32 inline math_ceil_f32(F32 value) {
    return glm::ceil(value);
}

F32 inline math_lerp_f32(F32 a, F32 b, F32 t) {
    return glm::mix(a, b, t);  // GLM's mix is equivalent to lerp
}

F64 inline math_lerp_f64(F64 a, F64 b, F64 t) {
    return glm::mix(a, b, t);  // GLM's mix is equivalent to lerp
}

F32 inline math_exp_f32(F32 value) {
    return glm::exp(value);
}

F32 inline math_pow_f32(F32 base, F32 exponent) {
    return glm::pow(base, exponent);
}

F32 inline math_atan2_f32(F32 y, F32 x) {
    return glm::atan(y, x);
}

F32 inline math_cos_f32(F32 angle) {
    return glm::cos(angle);
}

F32 inline math_acos_f32(F32 angle) {
    return glm::acos(angle);
}

F32 inline math_tan_f32(F32 angle) {
    return glm::tan(angle);
}

F32 inline math_asin_f32(F32 value) {
    return glm::asin(value);
}

F32 inline math_sin_f32(F32 x) {
    return glm::sin(x);
}

F64 inline math_sin_f64(F64 x) {
    return glm::sin(x);
}

void inline math_vec2_round(Vector2 *vec) {
    vec->x = math_round_f32(vec->x);
    vec->y = math_round_f32(vec->y);
}

void inline math_vec3_round(Vector3 *vec) {
    vec->x = math_round_f32(vec->x);
    vec->y = math_round_f32(vec->y);
    vec->z = math_round_f32(vec->z);
}

#define RANDOM_SEED 13371337

void inline random_seed(U32 seed) {
    SetRandomSeed(seed);
}

U8 inline random_u8(U8 min, U8 max) {
    return (U8)GetRandomValue(min, max);
}

S32 inline random_s32(S32 min, S32 max) {
    return GetRandomValue(min, max);
}

F32 inline random_f32(F32 min, F32 max) {
    return (F32)GetRandomValue((S32)(min * 1000), (S32)(max * 1000)) / 1000.0F;
}
