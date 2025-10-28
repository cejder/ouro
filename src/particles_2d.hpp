#pragma once

#include "color.hpp"
#include "common.hpp"
#include "array.hpp"
#include "scene.hpp"
#include "asset.hpp"

#include <raylib.h>

#define PARTICLES_2D_MAX 500'000
#define PARTICLES_2D_SPAWN_RATE_HISTORY_SIZE 128

// GPU-aligned particle structure (must match compute shader)
// Total size: 96 bytes for GPU alignment (multiple of 16)
struct Particle2D {
    Vector2 position;      // 8 bytes
    Vector2 velocity;      // 8 bytes
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
    F32 padding[3];        // 12 bytes - padding to align to 96 bytes (multiple of 16)
};

struct Particles2D {
    // Rendering
    U32 vao;
    AShader* draw_shader;
    AComputeShader* compute_shader;
    ATextureArray textures;        // Dynamic array of textures for indexing
    U64Array texture_handles;      // Bindless texture handles (GLuint64)

    // GPU data
    U32 ssbo;
    U32 texture_handles_ssbo;      // SSBO for bindless texture handles
    Particle2D* mapped_data;

    // Ring buffer state
    SZ write_index; // Where to write next particle

    // Debug tracking
    F32 spawn_rate_history[PARTICLES_2D_SPAWN_RATE_HISTORY_SIZE];   // Ring buffer of spawn rates (particles per second)
    SZ spawn_rate_index;                                             // Current position in spawn_rate_history
    SZ previous_write_index;                                         // Previous frame's write_index for delta calculation

    // Cached uniform locations - draw shader
    S32 draw_projection_loc;
    S32 draw_current_scene_draw_loc;

    // Cached uniform locations - compute shader
    S32 comp_delta_time_loc;
    S32 comp_time_loc;
    S32 comp_resolution_loc;
    S32 comp_particle_count_loc;
    S32 comp_current_scene_loc;
};

Particles2D extern g_particles2d;

void particles2d_init();
void particles2d_clear();
void particles2d_clear_scene(SceneType scene_type);
void particles2d_update(F32 dt);
void particles2d_draw();
U32 particles2d_register_texture(ATexture *texture);
void particles2d_add(Vector2 *positions,
                   Vector2 *velocities,
                   F32 *sizes,
                   Color *start_colors,
                   Color *end_colors,
                   F32 *lives,
                   U32 *texture_indices,
                   F32 *gravities,
                   F32 *rotation_speeds,
                   F32 *air_resistances,
                   SZ count);
void particles2d_add_effect_explosion(Rectangle spawn_rect, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles2d_add_effect_smoke    (Rectangle spawn_rect, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles2d_add_effect_sparkle  (Rectangle spawn_rect, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles2d_add_effect_fire     (Rectangle spawn_rect, Color start_color, Color end_color, F32 size_multiplier, SZ count);
void particles2d_add_effect_spiral   (Rectangle spawn_rect, Color start_color, Color end_color, F32 size_multiplier, SZ count);
