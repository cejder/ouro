#pragma once

#include "color.hpp"
#include "common.hpp"
#include "array.hpp"
#include "math.hpp"
#include "scene.hpp"
#include "asset.hpp"

#include <raylib.h>

#define PARTICLES_3D_MAX 500'000
#define PARTICLES_3D_SPAWN_RATE_HISTORY_SIZE 128

enum Particle3DBillboardMode : U32 { // NOLINT(performance-enum-size)
    PARTICLE3D_BILLBOARD_CAMERA_FACING    = 0,
    PARTICLE3D_BILLBOARD_VELOCITY_ALIGNED = 1,
    PARTICLE3D_BILLBOARD_Y_AXIS_LOCKED    = 2,
    PARTICLE3D_BILLBOARD_HORIZONTAL       = 3,  // Flat on ground (XZ plane)
};

// GPU-aligned particle structure (must match compute shader)
// Total size: 128 bytes for GPU alignment (multiple of 16)
struct Particle3D {
    Vector3 position;      // 12 bytes
    F32 padding0;          // 4 bytes - align to 16
    Vector3 velocity;      // 12 bytes
    F32 padding1;          // 4 bytes - align to 16
    Vector3 acceleration;  // 12 bytes - for custom forces (wind, explosions, etc.)
    F32 padding2;          // 4 bytes - align to 16
    ColorF start_color;    // 16 bytes (vec4 in shader)
    ColorF end_color;      // 16 bytes (vec4 in shader)
    F32 life;              // 4 bytes
    F32 initial_life;      // 4 bytes
    F32 size;              // 4 bytes
    F32 initial_size;      // 4 bytes
    U32 texture_index;     // 4 bytes
    F32 gravity;           // 4 bytes - per-particle gravity
    F32 rotation_speed;    // 4 bytes - rotation speed in radians/sec
    F32 air_resistance;    // 4 bytes - damping factor (0.0 = no damping, 1.0 = full damping)
    U32 scene_id;          // 4 bytes - which scene this particle belongs to
    U32 billboard_mode;    // 4 bytes - 0=camera-facing, 1=velocity-aligned, 2=Y-axis locked
    F32 stretch_factor;    // 4 bytes - velocity-based stretching multiplier
    F32 padding3;          // 4 bytes - padding to align to 128 bytes
};

struct Particles3D {
    // Rendering
    U32 vao;
    AShader* draw_shader;
    AComputeShader* compute_shader;
    ATextureArray textures;        // Dynamic array of textures for indexing
    U64Array texture_handles;      // Bindless texture handles (GLuint64)

    // GPU data
    U32 ssbo;
    U32 texture_handles_ssbo;      // SSBO for bindless texture handles
    Particle3D* mapped_data;

    // Ring buffer state
    SZ write_index; // Where to write next particle

    // Debug tracking
    F32 spawn_rate_history[PARTICLES_3D_SPAWN_RATE_HISTORY_SIZE];   // Ring buffer of spawn rates (particles per second)
    SZ spawn_rate_index;                                             // Current position in spawn_rate_history
    SZ previous_write_index;                                         // Previous frame's write_index for delta calculation

    // Cached uniform locations - draw shader
    S32 draw_view_proj_loc;
    S32 draw_camera_pos_loc;
    S32 draw_camera_right_loc;
    S32 draw_camera_up_loc;
    S32 draw_current_scene_draw_loc;

    // Cached uniform locations - compute shader
    S32 comp_delta_time_loc;
    S32 comp_time_loc;
    S32 comp_particle_count_loc;
    S32 comp_current_scene_loc;
};

Particles3D extern g_particles3d;

void particles3d_init();
void particles3d_clear();
void particles3d_clear_scene(SceneType scene_type);
void particles3d_update(F32 dt);
void particles3d_draw();
U32 particles3d_register_texture(ATexture *texture);

// Core particle spawning API
void particles3d_add(Vector3 *positions,
                   Vector3 *velocities,
                   Vector3 *accelerations,
                   F32 *sizes,
                   Color *start_colors,
                   Color *end_colors,
                   F32 *lives,
                   U32 *texture_indices,
                   F32 *gravities,
                   F32 *rotation_speeds,
                   F32 *air_resistances,
                   U32 *billboard_modes,
                   F32 *stretch_factors,
                   SZ count);

// Predefined particle effects
void particles3d_add_effect_explosion           (Vector3 center, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_smoke               (Vector3 origin, F32 spread, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_sparkle             (Vector3 center, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_fire                (Vector3 origin, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_spiral              (Vector3 center, F32 radius, F32 height, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_fountain            (Vector3 origin, F32 spread_angle, F32 power, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_trail               (Vector3 start, Vector3 velocity, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_dust_cloud          (Vector3 center, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_magic_burst         (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_debris              (Vector3 center, Vector3 impulse, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_ambient_rain        (Vector3 origin, F32 spread_radius, F32 fall_height, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_chaos_stress_test   (Vector3 center, F32 radius, SZ count);
void particles3d_add_effect_harvest_impact      (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_harvest_active      (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_harvest_complete    (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_selection_indicator (Vector3 position, F32 radius, Color start_color, Color end_color, SZ count);
void particles3d_add_effect_click_indicator     (Vector3 position, F32 radius, Color start_color, Color end_color, SZ count);
void particles3d_add_effect_blood_hit           (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_blood_death         (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles3d_add_effect_spawn               (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count);
