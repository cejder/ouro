#include "particles_3d.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "cvar.hpp"
#include "log.hpp"
#include "math.hpp"
#include "message.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "std.hpp"
#include "time.hpp"
#include "world.hpp"

#include <raymath.h>
#include <rlgl.h>
#include <external/glad.h>
#include <glm/gtc/type_ptr.hpp>

Particles3D g_particles3d = {};
Particle3DCommandQueue static g_particle3d_command_queue = {};

#ifndef __APPLE__

void particles3d_init() {
    // Initialize command queue mutex
    mtx_init(&g_particle3d_command_queue.mutex, mtx_plain);
    g_particle3d_command_queue.count = 0;
    // Load bindless texture extension
    if (!render_load_bindless_texture_extension()) { return; }

   // Initialize shaders and cache all uniform locations
    g_particles3d.draw_shader    = asset_get_shader("particles3d");
    g_particles3d.compute_shader = asset_get_compute_shader("particles3d");

    // Cache draw shader uniform locations
    g_particles3d.draw_view_proj_loc          = GetShaderLocation(g_particles3d.draw_shader->base, "u_view_proj");
    g_particles3d.draw_camera_pos_loc         = GetShaderLocation(g_particles3d.draw_shader->base, "u_camera_pos");
    g_particles3d.draw_camera_right_loc       = GetShaderLocation(g_particles3d.draw_shader->base, "u_camera_right");
    g_particles3d.draw_camera_up_loc          = GetShaderLocation(g_particles3d.draw_shader->base, "u_camera_up");
    g_particles3d.draw_current_scene_draw_loc = GetShaderLocation(g_particles3d.draw_shader->base, "u_current_scene");

    // Cache compute shader uniform locations
    g_particles3d.comp_delta_time_loc     = GetShaderLocation(g_particles3d.compute_shader->base, "u_delta_time");
    g_particles3d.comp_time_loc           = GetShaderLocation(g_particles3d.compute_shader->base, "u_time");
    g_particles3d.comp_particle_count_loc = GetShaderLocation(g_particles3d.compute_shader->base, "u_particle_count");
    g_particles3d.comp_current_scene_loc  = GetShaderLocation(g_particles3d.compute_shader->base, "u_current_scene");

    // Initialize dynamic texture array and bindless handles array
    array_init(MEMORY_TYPE_ARENA_PERMANENT, &g_particles3d.textures, 8);
    array_init(MEMORY_TYPE_ARENA_PERMANENT, &g_particles3d.texture_handles, 8);

    // Base quad vertices for instanced rendering (billboards)
    F32 const quad_vertices[] = {
        // Positions  // TexCoords
        -0.5F,  0.5F,   0.0F, 1.0F,   // Top-left
         0.5F, -0.5F,   1.0F, 0.0F,   // Bottom-right
        -0.5F, -0.5F,   0.0F, 0.0F,   // Bottom-left
        -0.5F,  0.5F,   0.0F, 1.0F,   // Top-left
         0.5F,  0.5F,   1.0F, 1.0F,   // Top-right
         0.5F, -0.5F,   1.0F, 0.0F    // Bottom-right
    };

    // Create VAO and quad VBO
    U32 quad_vbo = 0;
    glGenVertexArrays(1, &g_particles3d.vao);
    glGenBuffers(1, &quad_vbo);

    glBindVertexArray(g_particles3d.vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(F32), (void*)nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(F32), (void*)(2 * sizeof(F32)));

    // Create particle SSBO with persistent mapping
    glGenBuffers(1, &g_particles3d.ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_particles3d.ssbo);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(PARTICLES_3D_MAX * sizeof(Particle3D)),
                   nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    g_particles3d.mapped_data = (Particle3D*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                                          (GLsizeiptr)(PARTICLES_3D_MAX * sizeof(Particle3D)),
                                                          GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    if (!g_particles3d.mapped_data) {
        lle("Failed to map 3D particle buffer!");
        return;
    }

    // Create SSBO for bindless texture handles (allows unlimited textures)
    glGenBuffers(1, &g_particles3d.texture_handles_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_particles3d.texture_handles_ssbo);
    // Allocate initial space - will grow dynamically as textures are registered
    glBufferData(GL_SHADER_STORAGE_BUFFER, 8 * sizeof(GLuint64), nullptr, GL_DYNAMIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Initialize debug tracking
    g_particles3d.spawn_rate_index     = 0;
    g_particles3d.previous_write_index = 0;
    for (F32 &v : g_particles3d.spawn_rate_history) { v = 0.0F; }

    // Clear particle state to start fresh
    particles3d_clear();
}

void particles3d_clear() {
    // Reset write index
    g_particles3d.write_index = 0;

    // Clear all particle data in the mapped buffer and mark as dead
    if (g_particles3d.mapped_data) {
        for (SZ i = 0; i < PARTICLES_3D_MAX; ++i) {
            Particle3D* p = &g_particles3d.mapped_data[i];
            ou_memset(p, 0, sizeof(Particle3D));
            // Explicitly mark as dead to prevent rendering at origin
            p->life         = -1.0F;
            p->initial_life = 0.0F;
            p->size         = 0.0F;
        }
    }
}

void particles3d_clear_scene(SceneType scene_type) {
    // Clear only particles belonging to the specified scene
    if (g_particles3d.mapped_data) {
        for (SZ i = 0; i < PARTICLES_3D_MAX; ++i) {
            if (g_particles3d.mapped_data[i].scene_id == (U32)scene_type) {
                // Mark particle as dead by setting life to 0
                g_particles3d.mapped_data[i].life = 0.0F;
            }
        }
    }
}

void particles3d_update(F32 dt) {
    // Calculate spawn rate for debug tracking
    SZ const current_write_index  = g_particles3d.write_index;
    SZ const previous_write_index = g_particles3d.previous_write_index;
    SZ spawned_this_frame         = 0;

    // Handle ring buffer wrap-around when calculating delta
    if (current_write_index >= previous_write_index) {
        spawned_this_frame = current_write_index - previous_write_index;
    } else {
        spawned_this_frame = (PARTICLES_3D_MAX - previous_write_index) + current_write_index;
    }

    // Convert to particles per second (avoid division by zero)
    F32 const spawn_rate = (dt > 0.0F) ? (F32)spawned_this_frame / dt : 0.0F;

    // Store in history ring buffer
    g_particles3d.spawn_rate_history[g_particles3d.spawn_rate_index] = spawn_rate;
    g_particles3d.spawn_rate_index                                   = (g_particles3d.spawn_rate_index + 1) % PARTICLES_3D_SPAWN_RATE_HISTORY_SIZE;
    g_particles3d.previous_write_index                               = current_write_index;

    // Always update all particles (ring buffer)
    F32 const current_time       = time_get();
    U32 const particle_count     = (U32)PARTICLES_3D_MAX;
    // Use overlay scene if active, otherwise use current scene
    SceneType const active_scene = g_scenes.current_overlay_scene_type != SCENE_NONE ? g_scenes.current_overlay_scene_type : g_scenes.current_scene_type;
    U32 const current_scene      = (U32)active_scene;
    U32 const work_groups        = (U32)((PARTICLES_3D_MAX + 63) / 64);

    glUseProgram(g_particles3d.compute_shader->base.id);
    glUniform1f(g_particles3d.comp_delta_time_loc, dt);
    glUniform1f(g_particles3d.comp_time_loc, current_time);
    glUniform1ui(g_particles3d.comp_particle_count_loc, particle_count);
    glUniform1ui(g_particles3d.comp_current_scene_loc, current_scene);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, g_particles3d.ssbo);

    glDispatchCompute(work_groups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
}

void particles3d_draw() {
    Camera3D* camera = g_render.cameras.c3d.active_cam;

    glDisable(GL_CULL_FACE); // Disable face culling so particles are visible from both sides
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR); // Screen blend - softer merging
    // glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // Soft additive
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE); // Disable depth writes so particles can overlap
    glDepthFunc(GL_LEQUAL); // Less-or-equal depth test (matches what scene geometry uses)

    BeginShaderMode(g_particles3d.draw_shader->base);

    // Get the current view-projection matrix from Raylib's matrix stack
    // This ensures particles use the exact same transformation as other 3D geometry
    Matrix const view_matrix = rlGetMatrixModelview();
    Matrix const proj_matrix = rlGetMatrixProjection();
    Matrix const view_proj   = MatrixMultiply(view_matrix, proj_matrix);
    SetShaderValueMatrix(g_particles3d.draw_shader->base, g_particles3d.draw_view_proj_loc, view_proj);

    // Calculate camera right and up vectors for billboarding
    Vector3 const forward = Vector3Normalize(Vector3Subtract(camera->target, camera->position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera->up));
    Vector3 up = Vector3CrossProduct(right, forward);

    // Set camera uniforms
    SetShaderValue(g_particles3d.draw_shader->base, g_particles3d.draw_camera_pos_loc, &camera->position, SHADER_UNIFORM_VEC3);
    SetShaderValue(g_particles3d.draw_shader->base, g_particles3d.draw_camera_right_loc, &right, SHADER_UNIFORM_VEC3);
    SetShaderValue(g_particles3d.draw_shader->base, g_particles3d.draw_camera_up_loc, &up, SHADER_UNIFORM_VEC3);

    // Set current scene uniform (for scene-based particle filtering)
    SceneType const active_scene = g_scenes.current_overlay_scene_type != SCENE_NONE ? g_scenes.current_overlay_scene_type : g_scenes.current_scene_type;
    U32 current_scene_draw       = (U32)active_scene;
    SetShaderValue(g_particles3d.draw_shader->base, g_particles3d.draw_current_scene_draw_loc, &current_scene_draw, SHADER_UNIFORM_UINT);

    // Bind particle data SSBO (binding point 2)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, g_particles3d.ssbo);

    // Bind bindless texture handles SSBO (binding point 3)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, g_particles3d.texture_handles_ssbo);

    // Draw all particles using instancing - vertex shader culls dead particles
    glBindVertexArray(g_particles3d.vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (S32)PARTICLES_3D_MAX);

    glBindVertexArray(0);
    glEnable(GL_CULL_FACE); // Re-enable face culling
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE); // Re-enable depth writes

    EndShaderMode();

    INCREMENT_DRAW_CALL;
}

U32 particles3d_register_texture(ATexture *texture) {
    // Check if texture is already registered (avoid duplicates)
    for (SZ i = 0; i < g_particles3d.textures.count; ++i) {
        if (g_particles3d.textures.data[i] == texture) { return (U32)i; }
    }

    // Add texture to array
    array_push(&g_particles3d.textures, texture);
    U32 const index = (U32)(g_particles3d.textures.count - 1);

    // Create bindless handle for this texture (allows shader to access it by handle)
    U64 const handle = render_get_texture_handle(texture->base.id);
    if (handle == 0) {
        lle("Failed to get bindless texture handle for texture %u", texture->base.id);
        return 0;
    }

    // Make handle resident in GPU memory (required before shader can use it)
    render_make_texture_resident(handle);

    // Store handle in array
    array_push(&g_particles3d.texture_handles, handle);

    // Upload all handles to GPU SSBO so shaders can access them
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_particles3d.texture_handles_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(g_particles3d.texture_handles.count * sizeof(GLuint64)),
                 g_particles3d.texture_handles.data, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return index;
}

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
                   SZ count) {
    // Use overlay scene if active, otherwise use current scene
    SceneType const active_scene = g_scenes.current_overlay_scene_type != SCENE_NONE ? g_scenes.current_overlay_scene_type : g_scenes.current_scene_type;

    for (SZ i = 0; i < count; ++i) {
        ColorF const start_col = color_to_colorf(start_colors[i]);
        ColorF const end_col   = color_to_colorf(end_colors[i]);

        Particle3D *p = &g_particles3d.mapped_data[g_particles3d.write_index];
        p->position       = positions[i];
        p->velocity       = velocities[i];
        p->acceleration   = accelerations[i];
        p->start_color    = start_col;
        p->end_color      = end_col;
        p->initial_life   = lives[i];
        p->initial_size   = sizes[i];
        p->texture_index  = texture_indices[i];
        p->gravity        = gravities[i];
        p->rotation_speed = rotation_speeds[i];
        p->air_resistance = air_resistances[i];
        p->billboard_mode = billboard_modes[i];
        p->stretch_factor = stretch_factors[i];
        p->size           = sizes[i];
        p->life           = lives[i];
        p->scene_id       = (U32)active_scene;
        p->extra0         = 0.0F;
        p->extra1         = 0.0F;
        p->extra2         = 0.0F;
        p->extra3         = 0.0F;

        g_particles3d.write_index = (g_particles3d.write_index + 1) % PARTICLES_3D_MAX;
    }
}

void particles3d_add_explosion(Vector3 center, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    // Use multiple textures for variety
    C8 const* explosion_texture_names[] = {
        "particle_flare_01.png",
        "particle_spark_01.png",
        "particle_spark_03.png",
        "particle_circle_01.png",
        "particle_muzzle_01.png",
        "particle_explosion_01.png",
        "particle_explosion_02.png",
        "particle_flash_01.png"
    };
    SZ const texture_count = sizeof(explosion_texture_names) / sizeof(explosion_texture_names[0]);

    // Pre-load all textures into transient memory array
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(explosion_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        // Spherical explosion
        F32 const theta          = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const phi            = random_f32(0.0F, glm::pi<F32>());
        F32 const speed          = random_f32(2.0F, 25.0F);
        F32 const spawn_radius   = random_f32(0.0F, radius * 0.2F);

        Vector3 const dir = {
            math_sin_f32(phi) * math_cos_f32(theta),
            math_sin_f32(phi) * math_sin_f32(theta),
            math_cos_f32(phi)
        };

        positions[i]       = Vector3Add(center, Vector3Scale(dir, spawn_radius));
        velocities[i]      = Vector3Scale(dir, speed);
        accelerations[i]   = {0.0F, 0.0F, 0.0F};
        start_colors[i]    = color_variation(start_color, 25);
        end_colors[i]      = color_variation(end_color, 25);
        lives[i]           = random_f32(0.5F, 2.5F);
        sizes[i]           = random_f32(1.0F, 4.0F) * size_multiplier;
        gravities[i]       = random_f32(5.0F, 15.0F);
        rotation_speeds[i] = random_f32(-4.0F, 4.0F);
        air_resistances[i] = random_f32(0.01F, 0.03F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = random_f32(1.0F, 2.5F);

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_smoke(Vector3 origin, F32 spread, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    C8 const* smoke_texture_names[] = {
        "particle_black_smoke_00.png",
        "particle_black_smoke_05.png",
        "particle_black_smoke_10.png",
        "particle_black_smoke_15.png",
        "particle_black_smoke_20.png",
        "particle_fart_02.png",
        "particle_fart_05.png",
    };
    SZ const texture_count = sizeof(smoke_texture_names) / sizeof(smoke_texture_names[0]);

    // Pre-load all textures into transient memory array
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(smoke_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        F32 const drift_x = random_f32(-spread * 0.3F, spread * 0.3F);
        F32 const drift_z = random_f32(-spread * 0.3F, spread * 0.3F);
        F32 const rise    = random_f32(1.0F, 3.5F);

        positions[i] = {
            origin.x + random_f32(-spread, spread),
            origin.y + random_f32(0.0F, spread * 0.2F),
            origin.z + random_f32(-spread, spread)
        };
        velocities[i]      = {drift_x, rise, drift_z};
        accelerations[i]   = {0.0F, 0.0F, 0.0F};
        start_colors[i]    = color_variation(start_color, 10);
        end_colors[i]      = color_variation(end_color, 10);
        lives[i]           = random_f32(2.0F, 5.0F);
        sizes[i]           = random_f32(2.0F, 6.0F) * size_multiplier;
        gravities[i]       = random_f32(-3.0F, 1.0F); // Smoke rises
        rotation_speeds[i] = random_f32(-1.0F, 1.0F);
        air_resistances[i] = random_f32(0.02F, 0.05F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = 0.5F;

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_sparkle(Vector3 center, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    C8 const* sparkle_texture_names[] = {
        "particle_star_01.png",
        "particle_star_05.png",
        "particle_star_07.png",
        "particle_light_01.png",
        "particle_light_03.png",
        "particle_spark_05.png"
    };
    SZ const texture_count = sizeof(sparkle_texture_names) / sizeof(sparkle_texture_names[0]);

    // Pre-load all textures into transient memory array
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(sparkle_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        F32 const theta = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const phi   = random_f32(0.0F, glm::pi<F32>());
        F32 const r     = random_f32(0.0F, radius);
        F32 const speed = random_f32(0.5F, 3.0F);

        Vector3 const offset = {
            r * math_sin_f32(phi) * math_cos_f32(theta),
            r * math_sin_f32(phi) * math_sin_f32(theta),
            r * math_cos_f32(phi)
        };

        positions[i]       = Vector3Add(center, offset);
        velocities[i]      = Vector3Scale(Vector3Normalize(offset), speed);
        accelerations[i]   = {0.0F, 0.0F, 0.0F};
        start_colors[i]    = color_variation(start_color, 30);
        end_colors[i]      = color_variation(end_color, 30);
        lives[i]           = random_f32(1.0F, 4.0F);
        sizes[i]           = random_f32(0.5F, 2.0F) * size_multiplier;
        gravities[i]       = random_f32(2.0F, 5.0F);
        rotation_speeds[i] = random_f32(-3.0F, 3.0F);
        air_resistances[i] = random_f32(0.01F, 0.03F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = 0.3F;

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_fire(Vector3 origin, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    C8 const* fire_texture_names[] = {
        "particle_flame_01.png",
        "particle_flame_02.png",
        "particle_fire_01.png",
        "particle_fire_02.png",
        "particle_explosion_01.png",
    };
    SZ const texture_count = sizeof(fire_texture_names) / sizeof(fire_texture_names[0]);

    // Pre-load all textures into transient memory array
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(fire_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        F32 const flicker_x = random_f32(-radius * 0.5F, radius * 0.5F);
        F32 const flicker_z = random_f32(-radius * 0.5F, radius * 0.5F);
        F32 const rise = random_f32(2.0F, 6.0F);

        positions[i] = {
            origin.x + random_f32(-radius, radius),
            origin.y,
            origin.z + random_f32(-radius, radius)
        };
        velocities[i]      = {flicker_x, rise, flicker_z};
        accelerations[i]   = {0.0F, 0.0F, 0.0F};
        start_colors[i]    = color_variation(start_color, 20);
        end_colors[i]      = color_variation(end_color, 20);
        lives[i]           = random_f32(0.3F, 1.2F);
        sizes[i]           = random_f32(1.5F, 4.0F) * size_multiplier;
        gravities[i]       = random_f32(-5.0F, -2.0F); // Fire rises
        rotation_speeds[i] = random_f32(-2.0F, 2.0F);
        air_resistances[i] = random_f32(0.01F, 0.025F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = random_f32(0.8F, 1.5F);

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_spiral(Vector3 center, F32 radius, F32 height, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    C8 const* magic_texture_names[] = {
        "particle_magic_01.png",
        "particle_magic_03.png",
        "particle_magic_04.png",
        "particle_magic_05.png",
        "particle_symbol_01.png",
        "particle_symbol_02.png",
        "particle_twirl_01.png",
        "particle_twirl_02.png",
    };
    SZ const texture_count = sizeof(magic_texture_names) / sizeof(magic_texture_names[0]);

    // Pre-load all textures into transient memory array
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(magic_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        F32 const t             = (F32)i / (F32)count;
        F32 const angle         = t * 4.0F * glm::pi<F32>();
        F32 const spiral_radius = radius * t;
        F32 const spiral_height = height * t;

        positions[i] = {
            center.x + (spiral_radius * math_cos_f32(angle)),
            center.y + spiral_height,
            center.z + (spiral_radius * math_sin_f32(angle))
        };

        // Tangent velocity along spiral
        velocities[i] = {
            -spiral_radius * math_sin_f32(angle) * 2.0F,
            random_f32(1.0F, 3.0F),
            spiral_radius * math_cos_f32(angle) * 2.0F
        };
        accelerations[i]   = {0.0F, 0.0F, 0.0F};
        start_colors[i]    = color_variation(start_color, 20);
        end_colors[i]      = color_variation(end_color, 20);
        lives[i]           = random_f32(2.0F, 5.0F);
        sizes[i]           = random_f32(1.0F, 3.0F) * size_multiplier;
        gravities[i]       = random_f32(1.0F, 4.0F);
        rotation_speeds[i] = random_f32(-5.0F, 5.0F);
        air_resistances[i] = random_f32(0.01F, 0.02F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = 0.5F;

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_fountain(Vector3 origin, F32 spread_angle, F32 power, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    C8 const* fountain_texture_names[] = {
        "particle_circle_01.png",
        "particle_circle_03.png",
        "particle_light_01.png",
        "particle_spark_01.png"
    };
    SZ const texture_count = sizeof(fountain_texture_names) / sizeof(fountain_texture_names[0]);

    // Pre-load all textures into transient memory array
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(fountain_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        F32 const theta = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const phi   = random_f32((glm::pi<F32>() * 0.5F) - spread_angle, (glm::pi<F32>() * 0.5F) + spread_angle);
        F32 const speed = random_f32(power * 0.5F, power);

        positions[i] = origin;
        velocities[i] = {
            speed * math_sin_f32(phi) * math_cos_f32(theta),
            speed * math_cos_f32(phi),
            speed * math_sin_f32(phi) * math_sin_f32(theta)
        };
        accelerations[i]   = {0.0F, 0.0F, 0.0F};
        start_colors[i]    = color_variation(start_color, 15);
        end_colors[i]      = color_variation(end_color, 15);
        lives[i]           = random_f32(1.0F, 3.0F);
        sizes[i]           = random_f32(0.05F, 0.15F) * size_multiplier;
        gravities[i]       = random_f32(8.0F, 12.0F);
        rotation_speeds[i] = random_f32(-2.0F, 2.0F);
        air_resistances[i] = random_f32(0.005F, 0.015F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = random_f32(1.5F, 3.0F);

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_trail(Vector3 start, Vector3 velocity, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    C8 const* trail_texture_names[] = {
        "particle_trace_01.png",
        "particle_trace_03.png",
        "particle_trace_05.png",
        "particle_smoke_01.png"
    };
    SZ const texture_count = sizeof(trail_texture_names) / sizeof(trail_texture_names[0]);

    // Pre-load all textures into transient memory array
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(trail_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        positions[i]       = start;
        velocities[i]      = Vector3Add(velocity, (Vector3){random_f32(-0.5F, 0.5F), random_f32(-0.5F, 0.5F), random_f32(-0.5F, 0.5F)});
        accelerations[i]   = {0.0F, 0.0F, 0.0F};
        start_colors[i]    = color_variation(start_color, 10);
        end_colors[i]      = color_variation(end_color, 10);
        lives[i]           = random_f32(0.3F, 1.0F);
        sizes[i]           = random_f32(0.1F, 0.25F) * size_multiplier;
        gravities[i]       = random_f32(0.0F, 2.0F);
        rotation_speeds[i] = random_f32(-1.0F, 1.0F);
        air_resistances[i] = random_f32(0.02F, 0.04F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_VELOCITY_ALIGNED;
        stretch_factors[i] = random_f32(3.0F, 6.0F);

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_dust_cloud(Vector3 center, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    C8 const* dust_texture_names[] = {
        "particle_dirt_01.png",
        "particle_dirt_02.png",
        "particle_dirt_03.png",
        "particle_smoke_05.png",
        "particle_smoke_08.png"
    };
    SZ const texture_count = sizeof(dust_texture_names) / sizeof(dust_texture_names[0]);

    // Pre-load all textures into transient memory array
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(dust_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        F32 const theta = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const phi   = random_f32(0.0F, glm::pi<F32>());
        F32 const r     = random_f32(0.0F, radius);

        positions[i] = {
            center.x + (r * math_sin_f32(phi) * math_cos_f32(theta)),
            center.y + (r * math_sin_f32(phi) * math_sin_f32(theta)),
            center.z + (r * math_cos_f32(phi))
        };
        velocities[i]      = {random_f32(-1.0F, 1.0F), random_f32(0.5F, 2.0F), random_f32(-1.0F, 1.0F)};
        accelerations[i]   = {0.0F, 0.0F, 0.0F};
        start_colors[i]    = color_variation(start_color, 15);
        end_colors[i]      = color_variation(end_color, 15);
        lives[i]           = random_f32(1.5F, 4.0F);
        sizes[i]           = random_f32(0.2F, 0.5F) * size_multiplier;
        gravities[i]       = random_f32(1.0F, 3.0F);
        rotation_speeds[i] = random_f32(-1.5F, 1.5F);
        air_resistances[i] = random_f32(0.03F, 0.06F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = 0.5F;

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_magic_burst(Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    C8 const* magic_texture_names[] = {
        "particle_magic_01.png",
        "particle_magic_02.png",
        "particle_magic_03.png",
        "particle_magic_04.png",
        "particle_magic_05.png",
        "particle_star_01.png",
        "particle_star_05.png",
        "particle_flare_01.png"
    };
    SZ const texture_count = sizeof(magic_texture_names) / sizeof(magic_texture_names[0]);

    // Pre-load all textures into transient memory array
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(magic_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        F32 const theta = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const phi   = random_f32(0.0F, glm::pi<F32>());
        F32 const speed = random_f32(3.0F, 12.0F);

        Vector3 const dir = {
            math_sin_f32(phi) * math_cos_f32(theta),
            math_sin_f32(phi) * math_sin_f32(theta),
            math_cos_f32(phi)
        };

        positions[i]       = center;
        velocities[i]      = Vector3Scale(dir, speed);
        accelerations[i]   = Vector3Scale(dir, random_f32(-2.0F, -5.0F)); // Pull back towards center
        start_colors[i]    = color_variation(start_color, 25);
        end_colors[i]      = color_variation(end_color, 25);
        lives[i]           = random_f32(0.5F, 2.0F);
        sizes[i]           = random_f32(0.1F, 0.3F) * size_multiplier;
        gravities[i]       = 0.0F; // No gravity for magic
        rotation_speeds[i] = random_f32(-6.0F, 6.0F);
        air_resistances[i] = random_f32(0.01F, 0.02F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = random_f32(0.5F, 1.5F);

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_debris(Vector3 center, Vector3 impulse, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    C8 const* debris_texture_names[] = {
        "particle_dirt_01.png",
        "particle_dirt_02.png",
        "particle_scorch_01.png",
        "particle_scratch_01.png",
        "particle_slash_01.png"
    };
    SZ const texture_count = sizeof(debris_texture_names) / sizeof(debris_texture_names[0]);

    // Pre-load all textures into transient memory array
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(debris_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        F32 const scatter = 2.0F;
        positions[i]      = center;
        velocities[i]     = Vector3Add(impulse, (Vector3){
            random_f32(-scatter, scatter),
            random_f32(0.0F, scatter * 2.0F),
            random_f32(-scatter, scatter)
        });
        accelerations[i]   = {0.0F, 0.0F, 0.0F};
        start_colors[i]    = color_variation(start_color, 20);
        end_colors[i]      = color_variation(end_color, 20);
        lives[i]           = random_f32(1.0F, 3.0F);
        sizes[i]           = random_f32(0.05F, 0.2F) * size_multiplier;
        gravities[i]       = random_f32(10.0F, 18.0F);
        rotation_speeds[i] = random_f32(-8.0F, 8.0F);
        air_resistances[i] = random_f32(0.01F, 0.03F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = random_f32(1.0F, 2.0F);

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_ambient_rain(Vector3 origin, F32 spread_radius, F32 fall_height, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    // Clean rain effect implementation:
    // 1. Spawn particles at random heights within fall_height
    // 2. Give them initial downward velocity
    // 3. Apply gravity to accelerate downward
    // 4. Calculate exact time to reach ground (origin.y) = particle lifetime
    // 5. Color/size interpolates linearly over entire lifetime (shader handles this)

    C8 const* texture_names[] = {
        "particle_white_puff_00.png",
        "particle_white_puff_01.png",
        "particle_white_puff_02.png",
        "particle_white_puff_03.png",
        "particle_white_puff_04.png",
        "particle_white_puff_05.png",
        "particle_white_puff_06.png",
        "particle_white_puff_07.png",
        "particle_white_puff_08.png",
        "particle_white_puff_09.png",
        "particle_white_puff_10.png",
        "particle_white_puff_11.png",
        "particle_white_puff_12.png",
        "particle_white_puff_13.png",
        "particle_white_puff_14.png",
        "particle_white_puff_15.png",
        "particle_white_puff_16.png",
        "particle_white_puff_17.png",
        "particle_white_puff_18.png",
        "particle_white_puff_19.png",
        "particle_white_puff_20.png",
        "particle_white_puff_21.png",
        "particle_white_puff_22.png",
        "particle_white_puff_23.png",
        "particle_white_puff_24.png",
    };
    SZ const texture_count = sizeof(texture_names) / sizeof(texture_names[0]);

    // Pre-load textures
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(texture_names[i]);
    }

    // Allocate particle data arrays
    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    F32 const gravity_acceleration = 50.0F;  // Constant gravity for all rain particles

    F32 const partial_fall_height = 0.1F * fall_height;

    for (SZ i = 0; i < count; ++i) {
        // Spawn position: random point in circle, random height above ground
        // Use full height range (10 units minimum to avoid spawning at ground level)
        F32 const spawn_height = random_f32(fall_height - partial_fall_height, fall_height + partial_fall_height);
        F32 const angle = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const radius = random_f32(0.0F, spread_radius);

        positions[i] = {
            origin.x + (radius * math_cos_f32(angle)),
            origin.y + spawn_height,
            origin.z + (radius * math_sin_f32(angle))
        };

        // Initial velocity: downward with slight horizontal drift
        F32 const initial_downward_velocity = random_f32(8.0F, 15.0F);
        velocities[i] = {
            random_f32(-1.0F, 1.0F),
            -initial_downward_velocity,
            random_f32(-1.0F, 1.0F)
        };

        // No custom acceleration (gravity is handled separately)
        accelerations[i] = {0.0F, 0.0F, 0.0F};

        // Colors - apply very subtle variation for organic look
        // Keep variation minimal to preserve user-intended gradient
        start_colors[i] = color_variation(start_color, 8);
        end_colors[i] = color_variation(end_color, 8);

        // Physics parameters
        gravities[i] = gravity_acceleration;
        air_resistances[i] = 0.0F;  // CRITICAL: No air resistance for accurate physics calculation

        // Calculate lifetime using kinematic equation
        // Distance to fall: spawn_height (from spawn to ground at origin.y)
        // Initial velocity: initial_downward_velocity (downward)
        // Acceleration: gravity_acceleration (downward)
        // Kinematic equation: distance = v₀*t + 0.5*a*t²
        // Rearranged: 0.5*a*t² + v₀*t - distance = 0
        // Quadratic solution: t = (-v₀ + sqrt(v₀² + 2*a*distance)) / a
        F32 const v0 = initial_downward_velocity;
        F32 const a = gravity_acceleration;
        F32 const d = spawn_height;
        F32 const discriminant = (v0 * v0) + (2.0F * a * d);
        F32 const lifetime = (-v0 + math_sqrt_f32(discriminant)) / a;

        lives[i] = lifetime;

        // Visual parameters
        sizes[i] = random_f32(0.5F, 0.9F) * size_multiplier;
        rotation_speeds[i] = random_f32(-1.5F, 1.5F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = 0.0F;  // Keep constant size throughout lifetime

        // Assign random texture
        ATexture* texture = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_chaos_stress_test(Vector3 center, F32 radius, SZ count) {
    // ALL 157 particle textures for maximum stress testing!
    C8 const* all_texture_names[] = {
        "particle_black_smoke_00.png", "particle_black_smoke_01.png", "particle_black_smoke_02.png", "particle_black_smoke_03.png",
        "particle_black_smoke_04.png", "particle_black_smoke_05.png", "particle_black_smoke_06.png", "particle_black_smoke_07.png",
        "particle_black_smoke_08.png", "particle_black_smoke_09.png", "particle_black_smoke_10.png", "particle_black_smoke_11.png",
        "particle_black_smoke_12.png", "particle_black_smoke_13.png", "particle_black_smoke_14.png", "particle_black_smoke_15.png",
        "particle_black_smoke_16.png", "particle_black_smoke_17.png", "particle_black_smoke_18.png", "particle_black_smoke_19.png",
        "particle_black_smoke_20.png", "particle_black_smoke_21.png", "particle_black_smoke_22.png", "particle_black_smoke_23.png",
        "particle_black_smoke_24.png", "particle_circle_01.png", "particle_circle_02.png", "particle_circle_03.png",
        "particle_circle_04.png", "particle_circle_05.png", "particle_dirt_01.png", "particle_dirt_02.png",
        "particle_dirt_03.png", "particle_explosion_00.png", "particle_explosion_01.png", "particle_explosion_02.png",
        "particle_explosion_03.png", "particle_explosion_04.png", "particle_explosion_05.png", "particle_explosion_06.png",
        "particle_explosion_07.png", "particle_explosion_08.png", "particle_fart_00.png", "particle_fart_01.png",
        "particle_fart_02.png", "particle_fart_03.png", "particle_fart_04.png", "particle_fart_05.png",
        "particle_fart_06.png", "particle_fart_07.png", "particle_fart_08.png", "particle_fire_01.png",
        "particle_fire_02.png", "particle_flame_01.png", "particle_flame_02.png", "particle_flame_03.png",
        "particle_flame_04.png", "particle_flame_05.png", "particle_flame_06.png", "particle_flare_01.png",
        "particle_flash_00.png", "particle_flash_01.png", "particle_flash_02.png", "particle_flash_03.png",
        "particle_flash_04.png", "particle_flash_05.png", "particle_flash_06.png", "particle_flash_07.png",
        "particle_flash_08.png", "particle_light_01.png", "particle_light_02.png", "particle_light_03.png",
        "particle_magic_01.png", "particle_magic_02.png", "particle_magic_03.png", "particle_magic_04.png",
        "particle_magic_05.png", "particle_muzzle_01.png", "particle_muzzle_02.png", "particle_muzzle_03.png",
        "particle_muzzle_04.png", "particle_muzzle_05.png", "particle_scorch_01.png", "particle_scorch_02.png",
        "particle_scorch_03.png", "particle_scratch_01.png", "particle_slash_01.png", "particle_slash_02.png",
        "particle_slash_03.png", "particle_slash_04.png", "particle_smoke_01.png", "particle_smoke_02.png",
        "particle_smoke_03.png", "particle_smoke_04.png", "particle_smoke_05.png", "particle_smoke_06.png",
        "particle_smoke_07.png", "particle_smoke_08.png", "particle_smoke_09.png", "particle_smoke_10.png",
        "particle_spark_01.png", "particle_spark_02.png", "particle_spark_03.png", "particle_spark_04.png",
        "particle_spark_05.png", "particle_spark_06.png", "particle_spark_07.png", "particle_star_01.png",
        "particle_star_02.png", "particle_star_03.png", "particle_star_04.png", "particle_star_05.png",
        "particle_star_06.png", "particle_star_07.png", "particle_star_08.png", "particle_star_09.png",
        "particle_symbol_01.png", "particle_symbol_02.png", "particle_trace_01.png", "particle_trace_02.png",
        "particle_trace_03.png", "particle_trace_04.png", "particle_trace_05.png", "particle_trace_06.png",
        "particle_trace_07.png", "particle_twirl_01.png", "particle_twirl_02.png", "particle_twirl_03.png",
        "particle_white_puff_00.png", "particle_white_puff_01.png", "particle_white_puff_02.png", "particle_white_puff_03.png",
        "particle_white_puff_04.png", "particle_white_puff_05.png", "particle_white_puff_06.png", "particle_white_puff_07.png",
        "particle_white_puff_08.png", "particle_white_puff_09.png", "particle_white_puff_10.png", "particle_white_puff_11.png",
        "particle_white_puff_12.png", "particle_white_puff_13.png", "particle_white_puff_14.png", "particle_white_puff_15.png",
        "particle_white_puff_16.png", "particle_white_puff_17.png", "particle_white_puff_18.png", "particle_white_puff_19.png",
        "particle_white_puff_20.png", "particle_white_puff_21.png", "particle_white_puff_22.png", "particle_white_puff_23.png",
        "particle_white_puff_24.png", "particle_window_01.png", "particle_window_02.png", "particle_window_03.png",
        "particle_window_04.png"
    };
    SZ const texture_count = sizeof(all_texture_names) / sizeof(all_texture_names[0]);

    // Pre-load all textures into transient memory array
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(all_texture_names[i]);
    }

    auto *positions       = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities      = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations   = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors    = mcta(Color*,   count, sizeof(Color));
    auto *end_colors      = mcta(Color*,   count, sizeof(Color));
    F32* lives            = mcta(F32*,     count, sizeof(F32));
    F32* sizes            = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices  = mcta(U32*,     count, sizeof(U32));
    F32* gravities        = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds  = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances  = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes  = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors  = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        // Spherical spawn with variety
        F32 const theta = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const phi   = random_f32(0.0F, glm::pi<F32>());
        F32 const r     = random_f32(0.0F, radius);
        F32 const speed = random_f32(3.0F, 18.0F);

        Vector3 const dir = {
            math_sin_f32(phi) * math_cos_f32(theta),
            math_sin_f32(phi) * math_sin_f32(theta),
            math_cos_f32(phi)
        };

        Vector3 const spawn_offset = { r * dir.x, r * dir.y, r * dir.z };

        // Varied velocity patterns - always upward or outward initially
        F32 const pattern_choice = random_f32(0.0F, 1.0F);
        Vector3 velocity;
        if (pattern_choice < 0.3F) {
            // Radial explosion - ensure upward component
            Vector3 radial_dir = dir;
            if (radial_dir.y < 0.0F) { radial_dir.y = -radial_dir.y; } // Flip downward to upward
            velocity = Vector3Scale(radial_dir, speed);
        } else if (pattern_choice < 0.6F) {
            // Rising with drift - always positive Y
            velocity = (Vector3){random_f32(-speed * 0.3F, speed * 0.3F), speed * random_f32(0.5F, 1.2F), random_f32(-speed * 0.3F, speed * 0.3F)};
        } else {
            // Swirling/orbital - with upward bias
            Vector3 const tangent = { -dir.z, dir.y, dir.x };
            velocity = Vector3Add(Vector3Scale(dir, speed * 0.4F), Vector3Scale(tangent, speed * 0.6F));
            velocity.y = glm::abs(velocity.y); // Ensure upward
        }

        positions[i]       = Vector3Add(center, spawn_offset);
        velocities[i]      = velocity;
        accelerations[i]   = {random_f32(-2.0F, 2.0F), random_f32(-2.0F, 2.0F), random_f32(-2.0F, 2.0F)};

        // Varied color palettes instead of pure chaos
        F32 const hue_base = random_f32(0.0F, 1.0F);
        if (hue_base < 0.25F) {
            // Warm colors (reds, oranges, yellows)
            start_colors[i] = (Color){(U8)random_s32(200, 255), (U8)random_s32(100, 200), (U8)random_s32(0, 100), 255};
            end_colors[i]   = (Color){(U8)random_s32(100, 180), (U8)random_s32(0, 100), (U8)random_s32(0, 50), 0};
        } else if (hue_base < 0.5F) {
            // Cool colors (blues, cyans, purples)
            start_colors[i] = (Color){(U8)random_s32(0, 150), (U8)random_s32(100, 200), (U8)random_s32(200, 255), 255};
            end_colors[i]   = (Color){(U8)random_s32(0, 80), (U8)random_s32(0, 100), (U8)random_s32(100, 180), 0};
        } else if (hue_base < 0.75F) {
            // Greens and teals
            start_colors[i] = (Color){(U8)random_s32(0, 100), (U8)random_s32(200, 255), (U8)random_s32(100, 200), 255};
            end_colors[i]   = (Color){(U8)random_s32(0, 50), (U8)random_s32(100, 180), (U8)random_s32(0, 100), 0};
        } else {
            // Magentas and pinks
            start_colors[i] = (Color){(U8)random_s32(200, 255), (U8)random_s32(0, 150), (U8)random_s32(200, 255), 255};
            end_colors[i]   = (Color){(U8)random_s32(100, 180), (U8)random_s32(0, 80), (U8)random_s32(100, 180), 0};
        }

        lives[i]           = random_f32(1.5F, 6.0F);
        sizes[i]           = random_f32(0.5F, 4.0F);
        gravities[i]       = random_f32(-5.0F, 12.0F);
        rotation_speeds[i] = random_f32(-5.0F, 5.0F);
        air_resistances[i] = random_f32(0.01F, 0.04F);

        // Mostly camera-facing for consistent look
        U32 const mode_choice = (U32)random_s32(0, 9);
        billboard_modes[i] = mode_choice < 7 ? PARTICLE3D_BILLBOARD_CAMERA_FACING :
                            (mode_choice < 9 ? PARTICLE3D_BILLBOARD_VELOCITY_ALIGNED : PARTICLE3D_BILLBOARD_Y_AXIS_LOCKED);

        stretch_factors[i] = random_f32(0.5F, 2.5F);

        // Use ALL textures randomly
        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_harvest_impact(Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    C8 const* impact_texture_names[] = {
        "particle_muzzle_01.png",
        "particle_muzzle_02.png",
        "particle_muzzle_03.png",
        "particle_muzzle_04.png",
        "particle_muzzle_05.png",
    };
    SZ const texture_count = sizeof(impact_texture_names) / sizeof(impact_texture_names[0]);

    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(impact_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        F32 const theta = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const phi   = random_f32(0.0F, glm::pi<F32>() * 0.5F);
        F32 const speed = random_f32(5.0F, 15.0F);

        Vector3 const dir = {
            math_sin_f32(phi) * math_cos_f32(theta),
            math_cos_f32(phi),
            math_sin_f32(phi) * math_sin_f32(theta)
        };

        positions[i]       = Vector3Add(center, (Vector3){random_f32(-1.0F, 1.0F), random_f32(1.0F, 3.0F), random_f32(-1.0F, 1.0F)});
        velocities[i]      = Vector3Scale(dir, speed);
        accelerations[i]   = {0.0F, 0.0F, 0.0F};
        start_colors[i]    = color_variation(start_color, 30);
        end_colors[i]      = color_variation(end_color, 30);
        lives[i]           = random_f32(0.8F, 1.8F);
        sizes[i]           = random_f32(0.1F, 0.3F) * size_multiplier;
        gravities[i]       = random_f32(12.0F, 20.0F);
        rotation_speeds[i] = random_f32(-12.0F, 12.0F);
        air_resistances[i] = random_f32(0.015F, 0.035F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = random_f32(1.0F, 2.0F);

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_harvest_active(Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    C8 const* active_texture_names[] = {
        "particle_muzzle_01.png",
        "particle_muzzle_02.png",
        "particle_muzzle_03.png",
        "particle_muzzle_04.png",
        "particle_muzzle_05.png",
    };
    SZ const texture_count = sizeof(active_texture_names) / sizeof(active_texture_names[0]);

    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(active_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        F32 const theta = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const speed = random_f32(1.0F, 4.0F);

        positions[i] = Vector3Add(center, (Vector3){random_f32(-0.5F, 0.5F), random_f32(0.5F, 1.5F), random_f32(-0.5F, 0.5F)});
        velocities[i] = {
            math_cos_f32(theta) * speed,
            random_f32(2.0F, 5.0F),
            math_sin_f32(theta) * speed
        };
        accelerations[i]   = {0.0F, 0.0F, 0.0F};
        start_colors[i]    = color_variation(start_color, 20);
        end_colors[i]      = color_variation(end_color, 20);
        lives[i]           = random_f32(0.3F, 0.8F);
        sizes[i]           = random_f32(0.1F, 0.25F) * size_multiplier;
        gravities[i]       = random_f32(8.0F, 15.0F);
        rotation_speeds[i] = random_f32(-8.0F, 8.0F);
        air_resistances[i] = random_f32(0.02F, 0.04F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = random_f32(0.5F, 1.2F);

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_click_indicator(Vector3 position, F32 radius, Color start_color, Color end_color, SZ count) {
    // Fountain effect particles shooting upward
    C8 const* click_texture_names[] = {
        "particle_light_03.png",
        "particle_light_02.png",
        "particle_magic_02.png"
    };
    SZ const texture_count = sizeof(click_texture_names) / sizeof(click_texture_names[0]);

    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(click_texture_names[i]);
    }

    // More particles for fountain effect
    SZ const actual_count = glm::min(count, (SZ)20);

    auto *positions_arr     = mcta(Vector3*, actual_count, sizeof(Vector3));
    auto *velocities        = mcta(Vector3*, actual_count, sizeof(Vector3));
    auto *accelerations     = mcta(Vector3*, actual_count, sizeof(Vector3));
    auto *start_colors      = mcta(Color*,   actual_count, sizeof(Color));
    auto *end_colors        = mcta(Color*,   actual_count, sizeof(Color));
    F32* lives              = mcta(F32*,     actual_count, sizeof(F32));
    F32* sizes              = mcta(F32*,     actual_count, sizeof(F32));
    U32* texture_indices    = mcta(U32*,     actual_count, sizeof(U32));
    F32* gravities          = mcta(F32*,     actual_count, sizeof(F32));
    F32* rotation_speeds    = mcta(F32*,     actual_count, sizeof(F32));
    F32* air_resistances    = mcta(F32*,     actual_count, sizeof(F32));
    U32* billboard_modes    = mcta(U32*,     actual_count, sizeof(U32));
    F32* stretch_factors    = mcta(F32*,     actual_count, sizeof(F32));

    Vector3 terrain_normal = math_get_terrain_normal(g_world->base_terrain, position.x, position.z);
    F32 y_offset           = 0.15F * (1.0F - terrain_normal.y);

    for (SZ i = 0; i < actual_count; ++i) {
        // Start at ground level, slightly offset randomly
        F32 const radial_offset = random_f32(0.0F, radius * 0.5F);
        F32 const angle = random_f32(0.0F, 2.0F * glm::pi<F32>());

        positions_arr[i] = {
            position.x + radial_offset * math_cos_f32(angle),
            position.y + y_offset,
            position.z + radial_offset * math_sin_f32(angle)
        };

        // Fountain: shoot upward with slight outward velocity
        F32 const upward_speed = random_f32(3.0F, 6.0F);
        F32 const outward_speed = random_f32(0.5F, 1.5F);
        velocities[i] = {
            outward_speed * math_cos_f32(angle),
            upward_speed,
            outward_speed * math_sin_f32(angle)
        };
        accelerations[i]   = {0.0F, 0.0F, 0.0F};

        // Vibrant colors
        start_colors[i]    = color_variation(start_color, 15);
        end_colors[i]      = color_variation(end_color, 15);

        lives[i]           = random_f32(0.6F, 1.0F);  // Shorter life for fountain
        sizes[i]           = radius * random_f32(0.8F, 1.5F);  // Smaller particles
        gravities[i]       = random_f32(8.0F, 12.0F);  // Gravity pulls them down

        rotation_speeds[i] = random_f32(-10.0F, 10.0F);  // Spin as they fly

        air_resistances[i] = random_f32(0.01F, 0.03F);  // Some air resistance
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;  // Face camera (spherical billboard)
        stretch_factors[i] = 1.0F;

        // Use different textures for each particle
        ATexture* texture = textures[i % texture_count];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions_arr, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, actual_count);

    // Set terrain normal for all particles we just added
    for (SZ i = 0; i < actual_count; ++i) {
        SZ const particle_index = (g_particles3d.write_index - actual_count + i + PARTICLES_3D_MAX) % PARTICLES_3D_MAX;
        g_particles3d.mapped_data[particle_index].extra0 = terrain_normal.x;
        g_particles3d.mapped_data[particle_index].extra1 = terrain_normal.y;
        g_particles3d.mapped_data[particle_index].extra2 = terrain_normal.z;
    }
}

void particles3d_add_harvest_complete(Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    C8 const* complete_texture_names[] = {
        "particle_muzzle_01.png",
        "particle_muzzle_02.png",
        "particle_muzzle_03.png",
        "particle_muzzle_04.png",
        "particle_muzzle_05.png",
    };
    SZ const texture_count = sizeof(complete_texture_names) / sizeof(complete_texture_names[0]);

    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(complete_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        F32 const theta = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const phi   = random_f32(0.0F, glm::pi<F32>());
        F32 const speed = random_f32(8.0F, 20.0F);

        Vector3 const dir = {
            math_sin_f32(phi) * math_cos_f32(theta),
            math_sin_f32(phi) * math_sin_f32(theta),
            math_cos_f32(phi)
        };

        positions[i]       = Vector3Add(center, Vector3Scale(dir, random_f32(0.0F, 1.0F)));
        velocities[i]      = Vector3Scale(dir, speed);
        accelerations[i]   = {0.0F, 0.0F, 0.0F};
        start_colors[i]    = color_variation(start_color, 40);
        end_colors[i]      = color_variation(end_color, 40);
        lives[i]           = random_f32(1.0F, 2.5F);
        sizes[i]           = random_f32(0.3F, 0.8F) * size_multiplier;
        gravities[i]       = random_f32(5.0F, 12.0F);
        rotation_speeds[i] = random_f32(-15.0F, 15.0F);
        air_resistances[i] = random_f32(0.02F, 0.05F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = random_f32(1.5F, 3.0F);

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_blood_hit(Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    C8 const* blood_texture_names[] = {
        "particle_circle_01.png",
        "particle_circle_02.png",
        "particle_circle_03.png",
        "particle_circle_04.png",
        "particle_circle_05.png",
        "particle_smoke_01.png",
        "particle_smoke_02.png",
        "particle_smoke_03.png"
    };
    SZ const texture_count = sizeof(blood_texture_names) / sizeof(blood_texture_names[0]);

    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(blood_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        F32 const theta = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const phi   = random_f32(0.0F, glm::pi<F32>() * 0.5F);
        F32 const speed = random_f32(3.0F, 8.0F);

        Vector3 const dir = {
            math_sin_f32(phi) * math_cos_f32(theta),
            math_cos_f32(phi),
            math_sin_f32(phi) * math_sin_f32(theta)
        };

        positions[i]       = Vector3Add(center, (Vector3){random_f32(-0.3F, 0.3F), random_f32(0.5F, 1.5F), random_f32(-0.3F, 0.3F)});
        velocities[i]      = Vector3Scale(dir, speed);
        accelerations[i]   = {0.0F, 0.0F, 0.0F};
        start_colors[i]    = color_variation(start_color, 25);
        end_colors[i]      = color_variation(end_color, 25);
        lives[i]           = random_f32(0.5F, 1.2F);
        sizes[i]           = random_f32(0.15F, 0.4F) * size_multiplier;
        gravities[i]       = random_f32(15.0F, 25.0F);
        rotation_speeds[i] = random_f32(-10.0F, 10.0F);
        air_resistances[i] = random_f32(0.02F, 0.04F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = random_f32(1.0F, 1.8F);

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_blood_death(Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    C8 const* blood_texture_names[] = {
        "particle_circle_01.png",
        "particle_circle_02.png",
        "particle_circle_03.png",
        "particle_circle_04.png",
        "particle_circle_05.png",
        "particle_smoke_01.png",
        "particle_smoke_02.png",
        "particle_smoke_03.png",
        "particle_smoke_04.png",
        "particle_smoke_05.png",
        "particle_scorch_01.png",
        "particle_scorch_02.png"
    };
    SZ const texture_count = sizeof(blood_texture_names) / sizeof(blood_texture_names[0]);

    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(blood_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        F32 const theta = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const phi   = random_f32(0.0F, glm::pi<F32>());
        F32 const speed = random_f32(12.0F, 25.0F);

        Vector3 const dir = {
            math_sin_f32(phi) * math_cos_f32(theta),
            math_cos_f32(phi),
            math_sin_f32(phi) * math_sin_f32(theta)
        };

        positions[i]       = Vector3Add(center, (Vector3){random_f32(-0.5F, 0.5F), random_f32(0.5F, 2.0F), random_f32(-0.5F, 0.5F)});
        velocities[i]      = Vector3Scale(dir, speed);
        accelerations[i]   = {0.0F, 0.0F, 0.0F};
        start_colors[i]    = color_variation(start_color, 30);
        end_colors[i]      = color_variation(end_color, 30);
        lives[i]           = random_f32(1.0F, 2.0F);
        sizes[i]           = random_f32(0.25F, 0.7F) * size_multiplier;
        gravities[i]       = random_f32(10.0F, 20.0F);
        rotation_speeds[i] = random_f32(-15.0F, 15.0F);
        air_resistances[i] = random_f32(0.015F, 0.035F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = random_f32(1.5F, 3.0F);

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

void particles3d_add_spawn(Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    C8 const* spawn_texture_names[] = {
        "particle_star_01.png",
        "particle_star_05.png",
        "particle_star_07.png",
        "particle_light_01.png",
        "particle_light_03.png",
        "particle_magic_01.png",
        "particle_magic_03.png",
        "particle_spark_01.png",
        "particle_spark_05.png"
    };
    SZ const texture_count = sizeof(spawn_texture_names) / sizeof(spawn_texture_names[0]);

    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(spawn_texture_names[i]);
    }

    auto *positions        = mcta(Vector3*, count, sizeof(Vector3));
    auto *velocities       = mcta(Vector3*, count, sizeof(Vector3));
    auto *accelerations    = mcta(Vector3*, count, sizeof(Vector3));
    auto *start_colors     = mcta(Color*,   count, sizeof(Color));
    auto *end_colors       = mcta(Color*,   count, sizeof(Color));
    F32* lives             = mcta(F32*,     count, sizeof(F32));
    F32* sizes             = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices   = mcta(U32*,     count, sizeof(U32));
    F32* gravities         = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds   = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances   = mcta(F32*,     count, sizeof(F32));
    U32* billboard_modes   = mcta(U32*,     count, sizeof(U32));
    F32* stretch_factors   = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        // Spawn particles in a cylinder shape rising upward
        F32 const theta  = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const radius = random_f32(0.2F, 1.5F);
        F32 const height = random_f32(0.0F, 2.0F);

        Vector3 const offset = {
            radius * math_cos_f32(theta),
            height,
            radius * math_sin_f32(theta)
        };

        // Upward spiraling motion
        F32 const spiral_speed = random_f32(1.0F, 3.0F);
        Vector3 const tangent = {
            -math_sin_f32(theta) * spiral_speed,
            random_f32(3.0F, 6.0F),  // Strong upward velocity
            math_cos_f32(theta) * spiral_speed
        };

        positions[i]       = Vector3Add(center, offset);
        velocities[i]      = tangent;
        accelerations[i]   = {0.0F, 0.0F, 0.0F};
        start_colors[i]    = color_variation(start_color, 35);
        end_colors[i]      = color_variation(end_color, 35);
        lives[i]           = random_f32(1.5F, 3.0F);
        sizes[i]           = random_f32(0.8F, 2.5F) * size_multiplier;
        gravities[i]       = random_f32(-2.0F, 1.0F);  // Slight negative gravity for floating effect
        rotation_speeds[i] = random_f32(-8.0F, 8.0F);
        air_resistances[i] = random_f32(0.02F, 0.05F);
        billboard_modes[i] = PARTICLE3D_BILLBOARD_CAMERA_FACING;
        stretch_factors[i] = random_f32(0.3F, 0.6F);

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles3d_register_texture(texture);
    }

    particles3d_add(positions, velocities, accelerations, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, billboard_modes, stretch_factors, count);
}

// Thread-safe command queue functions (safe to call from worker threads)

void static i_particles3d_queue_command_full(Particle3DCommandType type, Vector3 center, Vector3 extra_vec, Color start_color, Color end_color, F32 param1, F32 param2, F32 size_multiplier, SZ count) {
    mtx_lock(&g_particle3d_command_queue.mutex);
    if (g_particle3d_command_queue.count < PARTICLES_3D_COMMAND_QUEUE_MAX) {
        Particle3DCommand *cmd = &g_particle3d_command_queue.commands[g_particle3d_command_queue.count++];
        cmd->type = type;
        cmd->center = center;
        cmd->extra_vec = extra_vec;
        cmd->start_color = start_color;
        cmd->end_color = end_color;
        cmd->param1 = param1;
        cmd->param2 = param2;
        cmd->size_multiplier = size_multiplier;
        cmd->count = count;
    } else {
        llt("Particle3D command queue full, dropping command");
    }
    mtx_unlock(&g_particle3d_command_queue.mutex);
}

void particles3d_queue_explosion(Vector3 center, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_EXPLOSION, center, {}, start_color, end_color, radius, 0.0F, size_multiplier, count);
}

void particles3d_queue_smoke(Vector3 origin, F32 spread, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_SMOKE, origin, {}, start_color, end_color, spread, 0.0F, size_multiplier, count);
}

void particles3d_queue_sparkle(Vector3 center, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_SPARKLE, center, {}, start_color, end_color, radius, 0.0F, size_multiplier, count);
}

void particles3d_queue_fire(Vector3 origin, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_FIRE, origin, {}, start_color, end_color, radius, 0.0F, size_multiplier, count);
}

void particles3d_queue_spiral(Vector3 center, F32 radius, F32 height, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_SPIRAL, center, {}, start_color, end_color, radius, height, size_multiplier, count);
}

void particles3d_queue_fountain(Vector3 origin, F32 spread_angle, F32 power, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_FOUNTAIN, origin, {}, start_color, end_color, spread_angle, power, size_multiplier, count);
}

void particles3d_queue_trail(Vector3 start, Vector3 velocity, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_TRAIL, start, velocity, start_color, end_color, 0.0F, 0.0F, size_multiplier, count);
}

void particles3d_queue_dust_cloud(Vector3 center, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_DUST_CLOUD, center, {}, start_color, end_color, radius, 0.0F, size_multiplier, count);
}

void particles3d_queue_magic_burst(Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_MAGIC_BURST, center, {}, start_color, end_color, 0.0F, 0.0F, size_multiplier, count);
}

void particles3d_queue_debris(Vector3 center, Vector3 impulse, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_DEBRIS, center, impulse, start_color, end_color, 0.0F, 0.0F, size_multiplier, count);
}

void particles3d_queue_ambient_rain(Vector3 origin, F32 spread_radius, F32 fall_height, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_AMBIENT_RAIN, origin, {}, start_color, end_color, spread_radius, fall_height, size_multiplier, count);
}

void particles3d_queue_chaos_stress_test(Vector3 center, F32 radius, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_CHAOS_STRESS_TEST, center, {}, {}, {}, radius, 0.0F, 0.0F, count);
}

void particles3d_queue_harvest_impact(Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_HARVEST_IMPACT, center, {}, start_color, end_color, 0.0F, 0.0F, size_multiplier, count);
}

void particles3d_queue_harvest_active(Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_HARVEST_ACTIVE, center, {}, start_color, end_color, 0.0F, 0.0F, size_multiplier, count);
}

void particles3d_queue_harvest_complete(Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_HARVEST_COMPLETE, center, {}, start_color, end_color, 0.0F, 0.0F, size_multiplier, count);
}

void particles3d_queue_click_indicator(Vector3 position, F32 radius, Color start_color, Color end_color, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_CLICK_INDICATOR, position, {}, start_color, end_color, radius, 0.0F, 0.0F, count);
}

void particles3d_queue_blood_hit(Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_BLOOD_HIT, center, {}, start_color, end_color, 0.0F, 0.0F, size_multiplier, count);
}

void particles3d_queue_blood_death(Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_BLOOD_DEATH, center, {}, start_color, end_color, 0.0F, 0.0F, size_multiplier, count);
}

void particles3d_queue_spawn(Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    i_particles3d_queue_command_full(PARTICLE3D_CMD_SPAWN, center, {}, start_color, end_color, 0.0F, 0.0F, size_multiplier, count);
}

// Main thread only: process all queued commands
void particles3d_process_command_queue() {
    mtx_lock(&g_particle3d_command_queue.mutex);
    U32 const cmd_count = g_particle3d_command_queue.count;
    mtx_unlock(&g_particle3d_command_queue.mutex);

    // Process commands without holding lock (main thread is the only processor)
    for (U32 i = 0; i < cmd_count; ++i) {
        Particle3DCommand const *cmd = &g_particle3d_command_queue.commands[i];

        switch (cmd->type) {
            case PARTICLE3D_CMD_EXPLOSION:
                particles3d_add_explosion(cmd->center, cmd->param1, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_SMOKE:
                particles3d_add_smoke(cmd->center, cmd->param1, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_SPARKLE:
                particles3d_add_sparkle(cmd->center, cmd->param1, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_FIRE:
                particles3d_add_fire(cmd->center, cmd->param1, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_SPIRAL:
                particles3d_add_spiral(cmd->center, cmd->param1, cmd->param2, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_FOUNTAIN:
                particles3d_add_fountain(cmd->center, cmd->param1, cmd->param2, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_TRAIL:
                particles3d_add_trail(cmd->center, cmd->extra_vec, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_DUST_CLOUD:
                particles3d_add_dust_cloud(cmd->center, cmd->param1, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_MAGIC_BURST:
                particles3d_add_magic_burst(cmd->center, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_DEBRIS:
                particles3d_add_debris(cmd->center, cmd->extra_vec, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_AMBIENT_RAIN:
                particles3d_add_ambient_rain(cmd->center, cmd->param1, cmd->param2, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_CHAOS_STRESS_TEST:
                particles3d_add_chaos_stress_test(cmd->center, cmd->param1, cmd->count);
                break;
            case PARTICLE3D_CMD_HARVEST_IMPACT:
                particles3d_add_harvest_impact(cmd->center, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_HARVEST_ACTIVE:
                particles3d_add_harvest_active(cmd->center, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_HARVEST_COMPLETE:
                particles3d_add_harvest_complete(cmd->center, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_CLICK_INDICATOR:
                particles3d_add_click_indicator(cmd->center, cmd->param1, cmd->start_color, cmd->end_color, cmd->count);
                break;
            case PARTICLE3D_CMD_BLOOD_HIT:
                particles3d_add_blood_hit(cmd->center, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_BLOOD_DEATH:
                particles3d_add_blood_death(cmd->center, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            case PARTICLE3D_CMD_SPAWN:
                particles3d_add_spawn(cmd->center, cmd->start_color, cmd->end_color, cmd->size_multiplier, cmd->count);
                break;
            default:
                break;
        }
    }

    // Clear queue
    mtx_lock(&g_particle3d_command_queue.mutex);
    g_particle3d_command_queue.count = 0;
    mtx_unlock(&g_particle3d_command_queue.mutex);
}

#else

void particles3d_init() { llw("Particle3D is not supported on macOS!"); }
void particles3d_clear() {}
void particles3d_clear_scene(SceneType scene_type) {}
void particles3d_update(F32 dt) {}
void particles3d_draw() {}
U32 particles3d_register_texture(ATexture *texture) {return 0;}
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
                   SZ count) {}
void particles3d_add_explosion           (Vector3 center, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_smoke               (Vector3 origin, F32 spread, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_sparkle             (Vector3 center, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_fire                (Vector3 origin, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_spiral              (Vector3 center, F32 radius, F32 height, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_fountain            (Vector3 origin, F32 spread_angle, F32 power, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_trail               (Vector3 start, Vector3 velocity, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_dust_cloud          (Vector3 center, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_magic_burst         (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_debris              (Vector3 center, Vector3 impulse, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_ambient_rain        (Vector3 origin, F32 spread_radius, F32 fall_height, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_chaos_stress_test   (Vector3 center, F32 radius, SZ count) {}
void particles3d_add_harvest_impact      (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_harvest_active      (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_harvest_complete    (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_click_indicator     (Vector3 position, F32 radius, Color start_color, Color end_color, SZ count) {}
void particles3d_add_blood_hit           (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_blood_death         (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_add_spawn               (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}

void particles3d_queue_explosion         (Vector3 center, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_smoke             (Vector3 origin, F32 spread, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_sparkle           (Vector3 center, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_fire              (Vector3 origin, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_spiral            (Vector3 center, F32 radius, F32 height, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_fountain          (Vector3 origin, F32 spread_angle, F32 power, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_trail             (Vector3 start, Vector3 velocity, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_dust_cloud        (Vector3 center, F32 radius, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_magic_burst       (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_debris            (Vector3 center, Vector3 impulse, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_ambient_rain      (Vector3 origin, F32 spread_radius, F32 fall_height, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_chaos_stress_test (Vector3 center, F32 radius, SZ count) {}
void particles3d_queue_harvest_impact    (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_harvest_active    (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_harvest_complete  (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_click_indicator   (Vector3 position, F32 radius, Color start_color, Color end_color, SZ count) {}
void particles3d_queue_blood_hit         (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_blood_death       (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_queue_spawn             (Vector3 center, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles3d_process_command_queue   () {}

#endif
