#include "particles_2d.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "cvar.hpp"
#include "log.hpp"
#include "math.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "std.hpp"
#include "time.hpp"

#include <raymath.h>
#include <external/glad.h>
#include <glm/gtc/type_ptr.hpp>

Particles2D g_particles2d = {};

#if !defined(__APPLE__)

void particles2d_init() {
    // Load bindless texture extension
    if (!render_load_bindless_texture_extension()) { return; }

   // Initialize shaders and cache all uniform locations
    g_particles2d.draw_shader       = asset_get_shader("particles2d");
    g_particles2d.compute_shader    = asset_get_compute_shader("particles2d");

    // Cache draw shader uniform locations
    g_particles2d.draw_projection_loc         = GetShaderLocation(g_particles2d.draw_shader->base, "u_projection");
    g_particles2d.draw_current_scene_draw_loc = GetShaderLocation(g_particles2d.draw_shader->base, "u_current_scene");

    // Cache compute shader uniform locations
    g_particles2d.comp_delta_time_loc     = GetShaderLocation(g_particles2d.compute_shader->base, "u_delta_time");
    g_particles2d.comp_time_loc           = GetShaderLocation(g_particles2d.compute_shader->base, "u_time");
    g_particles2d.comp_resolution_loc     = GetShaderLocation(g_particles2d.compute_shader->base, "u_resolution");
    g_particles2d.comp_particle_count_loc = GetShaderLocation(g_particles2d.compute_shader->base, "u_particle_count");
    g_particles2d.comp_current_scene_loc  = GetShaderLocation(g_particles2d.compute_shader->base, "u_current_scene");

    // Initialize dynamic texture array and bindless handles array
    array_init(MEMORY_TYPE_ARENA_PERMANENT, &g_particles2d.textures, 8);
    array_init(MEMORY_TYPE_ARENA_PERMANENT, &g_particles2d.texture_handles, 8);

    // Base quad vertices for instanced rendering
    F32 const quad_vertices[] = {
        // Positions  // TexCoords
        0.0F, 1.0F,   0.0F, 1.0F,   // Top-left
        1.0F, 0.0F,   1.0F, 0.0F,   // Bottom-right
        0.0F, 0.0F,   0.0F, 0.0F,   // Bottom-left
        0.0F, 1.0F,   0.0F, 1.0F,   // Top-left
        1.0F, 1.0F,   1.0F, 1.0F,   // Top-right
        1.0F, 0.0F,   1.0F, 0.0F    // Bottom-right
    };

    // Create VAO and quad VBO
    U32 quad_vbo = 0;
    glGenVertexArrays(1, &g_particles2d.vao);
    glGenBuffers(1, &quad_vbo);

    glBindVertexArray(g_particles2d.vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(F32), (void*)nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(F32), (void*)(2 * sizeof(F32)));

    // Create particle SSBO with persistent mapping
    glGenBuffers(1, &g_particles2d.ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_particles2d.ssbo);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(PARTICLES_2D_MAX * sizeof(Particle2D)),
                   nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    g_particles2d.mapped_data = (Particle2D*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                                          (GLsizeiptr)(PARTICLES_2D_MAX * sizeof(Particle2D)),
                                                          GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    if (!g_particles2d.mapped_data) {
        lle("Failed to map particle buffer!");
        return;
    }

    // Create SSBO for bindless texture handles (allows unlimited textures)
    glGenBuffers(1, &g_particles2d.texture_handles_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_particles2d.texture_handles_ssbo);
    // Allocate initial space - will grow dynamically as textures are registered
    glBufferData(GL_SHADER_STORAGE_BUFFER, 8 * sizeof(GLuint64), nullptr, GL_DYNAMIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Initialize debug tracking
    g_particles2d.spawn_rate_index     = 0;
    g_particles2d.previous_write_index = 0;
    for (F32 &v : g_particles2d.spawn_rate_history) { v = 0.0F; }

    // Clear particle state to start fresh
    particles2d_clear();
}

void particles2d_clear() {
    // Reset write index
    g_particles2d.write_index = 0;

    // Clear all particle data in the mapped buffer and mark as dead
    if (g_particles2d.mapped_data) {
        for (SZ i = 0; i < PARTICLES_2D_MAX; ++i) {
            Particle2D* p = &g_particles2d.mapped_data[i];
            ou_memset(p, 0, sizeof(Particle2D));
            // Explicitly mark as dead to prevent rendering at origin
            p->life         = -1.0F;
            p->initial_life = 0.0F;
            p->size         = 0.0F;
        }
    }
}

void particles2d_clear_scene(SceneType scene_type) {
    // Clear only particles belonging to the specified scene
    if (g_particles2d.mapped_data) {
        for (SZ i = 0; i < PARTICLES_2D_MAX; ++i) {
            if (g_particles2d.mapped_data[i].scene_id == (U32)scene_type) {
                // Mark particle as dead by setting life to 0
                g_particles2d.mapped_data[i].life = 0.0F;
            }
        }
    }
}

void particles2d_update(F32 dt) {
    // Calculate spawn rate for debug tracking
    SZ const current_write_index  = g_particles2d.write_index;
    SZ const previous_write_index = g_particles2d.previous_write_index;
    SZ spawned_this_frame         = 0;

    // Handle ring buffer wrap-around when calculating delta
    if (current_write_index >= previous_write_index) {
        spawned_this_frame = current_write_index - previous_write_index;
    } else {
        spawned_this_frame = (PARTICLES_2D_MAX - previous_write_index) + current_write_index;
    }

    // Convert to particles per second (avoid division by zero)
    F32 const spawn_rate = (dt > 0.0F) ? (F32)spawned_this_frame / dt : 0.0F;

    // Store in history ring buffer
    g_particles2d.spawn_rate_history[g_particles2d.spawn_rate_index] = spawn_rate;
    g_particles2d.spawn_rate_index                                   = (g_particles2d.spawn_rate_index + 1) % PARTICLES_2D_SPAWN_RATE_HISTORY_SIZE;
    g_particles2d.previous_write_index                               = current_write_index;

    // Always update all particles (ring buffer)
    F32 const current_time       = time_get();
    U32 const particle_count     = (U32)PARTICLES_2D_MAX;
    // Use overlay scene if active, otherwise use current scene
    SceneType const active_scene = g_scenes.current_overlay_scene_type != SCENE_NONE ? g_scenes.current_overlay_scene_type : g_scenes.current_scene_type;
    U32 const current_scene      = (U32)active_scene;
    U32 const work_groups        = (U32)((PARTICLES_2D_MAX + 63) / 64);

    glUseProgram(g_particles2d.compute_shader->base.id);
    glUniform1f(g_particles2d.comp_delta_time_loc, dt);
    glUniform1f(g_particles2d.comp_time_loc, current_time);
    glUniform2f(g_particles2d.comp_resolution_loc, (F32)c_video__render_resolution_width, (F32)c_video__render_resolution_height);
    glUniform1ui(g_particles2d.comp_particle_count_loc, particle_count);
    glUniform1ui(g_particles2d.comp_current_scene_loc, current_scene);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, g_particles2d.ssbo);

    glDispatchCompute(work_groups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
}

void particles2d_draw() {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    BeginShaderMode(g_particles2d.draw_shader->base);

    // Set projection matrix uniform
    Matrix const projection = MatrixOrtho(0.0F, (F32)c_video__render_resolution_width, (F32)c_video__render_resolution_height, 0.0F, -1.0F, 1.0F);
    SetShaderValueMatrix(g_particles2d.draw_shader->base, g_particles2d.draw_projection_loc, projection);

    // Set current scene uniform (for scene-based particle filtering)
    SceneType const active_scene = g_scenes.current_overlay_scene_type != SCENE_NONE ? g_scenes.current_overlay_scene_type : g_scenes.current_scene_type;
    U32 current_scene_draw       = (U32)active_scene;
    SetShaderValue(g_particles2d.draw_shader->base, g_particles2d.draw_current_scene_draw_loc, &current_scene_draw, SHADER_UNIFORM_UINT);

    // Bind particle data SSBO (binding point 2)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, g_particles2d.ssbo);

    // Bind bindless texture handles SSBO (binding point 3)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, g_particles2d.texture_handles_ssbo);

    // Draw all particles using instancing - vertex shader culls dead particles
    glBindVertexArray(g_particles2d.vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (S32)PARTICLES_2D_MAX);

    glBindVertexArray(0);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    EndShaderMode();

    g_render.rmode_data[g_render.begun_rmode].draw_call_count++;
}

U32 particles2d_register_texture(ATexture *texture) {
    // Check if texture is already registered (avoid duplicates)
    for (SZ i = 0; i < g_particles2d.textures.count; ++i) {
        if (g_particles2d.textures.data[i] == texture) { return (U32)i; }
    }

    // Add texture to array
    array_push(&g_particles2d.textures, texture);
    U32 const index = (U32)(g_particles2d.textures.count - 1);

    // Create bindless handle for this texture (allows shader to access it by handle)
    U64 const handle = render_get_texture_handle(texture->base.id);
    if (handle == 0) {
        lle("Failed to get bindless texture handle for texture %u", texture->base.id);
        return 0;
    }

    // Make handle resident in GPU memory (required before shader can use it)
    render_make_texture_resident(handle);

    // Store handle in array
    array_push(&g_particles2d.texture_handles, handle);

    // Upload all handles to GPU SSBO so shaders can access them
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_particles2d.texture_handles_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(g_particles2d.texture_handles.count * sizeof(GLuint64)),
                 g_particles2d.texture_handles.data, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return index;
}

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
                   SZ count) {
    // Use overlay scene if active, otherwise use current scene
    SceneType const active_scene = g_scenes.current_overlay_scene_type != SCENE_NONE ? g_scenes.current_overlay_scene_type : g_scenes.current_scene_type;

    for (SZ i = 0; i < count; ++i) {
        ColorF const start_col = color_to_colorf(start_colors[i]);
        ColorF const end_col   = color_to_colorf(end_colors[i]);

        Particle2D *p = &g_particles2d.mapped_data[g_particles2d.write_index];
        p->position       = positions[i];
        p->velocity       = velocities[i];
        p->start_color    = start_col;
        p->end_color      = end_col;
        p->initial_life   = lives[i];
        p->initial_size   = sizes[i];
        p->texture_index  = texture_indices[i];
        p->gravity        = gravities[i];
        p->rotation_speed = rotation_speeds[i];
        p->air_resistance = air_resistances[i];
        p->size           = sizes[i];
        p->life           = lives[i];
        p->scene_id       = (U32)active_scene;

        g_particles2d.write_index = (g_particles2d.write_index + 1) % PARTICLES_2D_MAX;
    }
}

void particles2d_add_effect_explosion(Rectangle spawn_rect, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    // Use multiple textures for variety
    C8 const* explosion_texture_names[] = {
        "particle_flare_01.png",
        "particle_spark_01.png",
        "particle_spark_03.png",
        "particle_circle_01.png",
        "particle_muzzle_01.png"
    };

    SZ const texture_count = sizeof(explosion_texture_names) / sizeof(explosion_texture_names[0]);

    // Pre-load all textures into transient memory array
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(explosion_texture_names[i]);
    }

    auto *positions       = mcta(Vector2*, count, sizeof(Vector2));
    auto *velocities      = mcta(Vector2*, count, sizeof(Vector2));
    auto *start_colors    = mcta(Color*,   count, sizeof(Color));
    auto *end_colors      = mcta(Color*,   count, sizeof(Color));
    F32* lives            = mcta(F32*,     count, sizeof(F32));
    F32* sizes            = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices  = mcta(U32*,     count, sizeof(U32));
    F32* gravities        = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds  = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances  = mcta(F32*,     count, sizeof(F32));

    Vector2 const center = {spawn_rect.x + (spawn_rect.width * 0.5F), spawn_rect.y + (spawn_rect.height * 0.5F)};

    for (SZ i = 0; i < count; ++i) {
        F32 const angle           = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const speed           = random_f32(50.0F, 1200.0F);
        F32 const distance_factor = random_f32(0.3F, 1.0F);

        // Spawn from center with some spread
        F32 const spawn_radius = random_f32(0.0F, glm::max(spawn_rect.width, spawn_rect.height) * 0.2F);
        F32 const spawn_angle  = random_f32(0.0F, 2.0F * glm::pi<F32>());
        positions[i] = {
            center.x + (math_cos_f32(spawn_angle) * spawn_radius),
            center.y + (math_sin_f32(spawn_angle) * spawn_radius)
        };

        velocities[i]      = {math_cos_f32(angle) * speed * distance_factor, math_sin_f32(angle) * speed * distance_factor};
        start_colors[i]    = color_variation(start_color, 25);
        end_colors[i]      = color_variation(end_color, 25);
        lives[i]           = random_f32(0.4F, 2.8F);
        sizes[i]           = ui_scale_y(random_f32(0.5F, 1.5F)) * size_multiplier * random_f32(0.8F, 1.4F);
        gravities[i]       = random_f32(150.0F, 300.0F);
        rotation_speeds[i] = random_f32(-4.0F, 4.0F);
        air_resistances[i] = random_f32(0.005F, 0.02F);

        // Random texture selection
        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles2d_register_texture(texture);
    }

    particles2d_add(positions, velocities, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, count);
}

void particles2d_add_effect_smoke(Rectangle spawn_rect, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    // Use varied smoke textures for realistic effect
    C8 const* smoke_texture_names[] = {
        "particle_black_smoke_00.png",
        "particle_black_smoke_01.png",
        "particle_black_smoke_02.png",
        "particle_black_smoke_03.png",
        "particle_black_smoke_04.png",
        "particle_black_smoke_05.png",
        "particle_black_smoke_06.png",
        "particle_black_smoke_07.png",
        "particle_black_smoke_08.png",
        "particle_black_smoke_09.png",
        "particle_black_smoke_10.png",
        "particle_black_smoke_11.png",
        "particle_black_smoke_12.png",
        "particle_black_smoke_13.png",
        "particle_black_smoke_14.png",
        "particle_black_smoke_15.png",
        "particle_black_smoke_16.png",
        "particle_black_smoke_17.png",
        "particle_black_smoke_18.png",
        "particle_black_smoke_19.png",
        "particle_black_smoke_20.png",
        "particle_black_smoke_21.png",
        "particle_black_smoke_22.png",
        "particle_black_smoke_23.png",
        "particle_black_smoke_24.png",
        "particle_fart_00.png",
        "particle_fart_01.png",
        "particle_fart_02.png",
        "particle_fart_03.png",
        "particle_fart_04.png",
        "particle_fart_05.png",
        "particle_fart_06.png",
        "particle_fart_07.png",
        "particle_fart_08.png",
    };

    SZ const texture_count = sizeof(smoke_texture_names) / sizeof(smoke_texture_names[0]);

    // Pre-load all textures into transient memory array
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(smoke_texture_names[i]);
    }

    auto *positions       = mcta(Vector2*, count, sizeof(Vector2));
    auto *velocities      = mcta(Vector2*, count, sizeof(Vector2));
    auto *start_colors    = mcta(Color*,   count, sizeof(Color));
    auto *end_colors      = mcta(Color*,   count, sizeof(Color));
    F32* lives            = mcta(F32*,     count, sizeof(F32));
    F32* sizes            = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices  = mcta(U32*,     count, sizeof(U32));
    F32* gravities        = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds  = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances  = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        F32 const drift        = random_f32(-60.0F, 60.0F);
        F32 const upward_speed = random_f32(-200.0F, -40.0F);
        F32 const turbulence   = random_f32(-20.0F, 20.0F);

        positions[i] = {
            spawn_rect.x + random_f32(0.0F, spawn_rect.width) + random_f32(-5.0F, 5.0F),
            spawn_rect.y + random_f32(0.0F, spawn_rect.height)
        };

        velocities[i]      = {drift + turbulence, upward_speed};
        start_colors[i]    = color_variation(start_color, 5);
        end_colors[i]      = color_variation(end_color, 5);
        lives[i]           = random_f32(1.5F, 4.0F);
        sizes[i]           = ui_scale_y(random_f32(0.5F, 1.5F)) * size_multiplier;
        gravities[i]       = random_f32(-50.0F, 30.0F);  // Smoke rises so negative/low gravity
        rotation_speeds[i] = random_f32(-1.0F, 1.0F);    // Slow rotation for smoke
        air_resistances[i] = random_f32(0.02F, 0.05F);   // Higher air resistance for smoke

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles2d_register_texture(texture);
    }

    particles2d_add(positions, velocities, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, count);
}

void particles2d_add_effect_sparkle(Rectangle spawn_rect, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    // Mix of sparkly textures for magical effect
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

    auto *positions       = mcta(Vector2*, count, sizeof(Vector2));
    auto *velocities      = mcta(Vector2*, count, sizeof(Vector2));
    auto *start_colors    = mcta(Color*,   count, sizeof(Color));
    auto *end_colors      = mcta(Color*,   count, sizeof(Color));
    F32* lives            = mcta(F32*,     count, sizeof(F32));
    F32* sizes            = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices  = mcta(U32*,     count, sizeof(U32));
    F32* gravities        = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds  = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances  = mcta(F32*,     count, sizeof(F32));

    Vector2 const center = {spawn_rect.x + (spawn_rect.width * 0.5F), spawn_rect.y + (spawn_rect.height * 0.5F)};

    for (SZ i = 0; i < count; ++i) {
        F32 const angle          = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const speed          = random_f32(10.0F, 180.0F);
        F32 const twinkle_factor = random_f32(0.5F, 1.5F);

        // Create sparkles in clusters and scattered patterns
        F32 const cluster_chance = random_f32(0.0F, 1.0F);
        Vector2 spawn_pos;
        if (cluster_chance < 0.6F) {
            // Clustered sparkles
            F32 const orbit_radius = glm::max(spawn_rect.width, spawn_rect.height) * 0.3F;
            spawn_pos = {
                center.x + random_f32(-orbit_radius, orbit_radius),
                center.y + random_f32(-orbit_radius, orbit_radius)
            };
        } else {
            // Scattered sparkles
            spawn_pos = {
                spawn_rect.x + random_f32(0.0F, spawn_rect.width),
                spawn_rect.y + random_f32(0.0F, spawn_rect.height)
            };
        }

        positions[i] = spawn_pos;
        velocities[i] = {
            math_cos_f32(angle) * speed * 0.4F * twinkle_factor,
            math_sin_f32(angle) * speed * 0.4F * twinkle_factor
        };

        start_colors[i]    = color_variation(start_color, 30);
        end_colors[i]      = color_variation(end_color, 30);
        lives[i]           = random_f32(0.8F, 4.0F);
        sizes[i]           = ui_scale_y(random_f32(0.25F, 1.0F)) * size_multiplier * twinkle_factor;
        gravities[i]       = random_f32(50.0F, 120.0F);  // Light gravity for sparkles
        rotation_speeds[i] = random_f32(-3.0F, 3.0F);    // Sparkly rotation
        air_resistances[i] = random_f32(0.01F, 0.03F);   // Low air resistance

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles2d_register_texture(texture);
    }

    particles2d_add(positions, velocities, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, count);
}

void particles2d_add_effect_fire(Rectangle spawn_rect, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    // Mix flame and fire textures for realistic fire effect
    C8 const* fire_texture_names[] = {
        "particle_flame_01.png",
        "particle_flame_02.png",
        "particle_fire_01.png",
        "particle_fire_02.png",
        "particle_explosion_01.png",
        "particle_explosion_02.png",
        "particle_black_smoke_01.png",
        "particle_black_smoke_02.png",
    };

    SZ const texture_count = sizeof(fire_texture_names) / sizeof(fire_texture_names[0]);

    // Pre-load all textures into transient memory array
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(fire_texture_names[i]);
    }

    auto *positions       = mcta(Vector2*, count, sizeof(Vector2));
    auto *velocities      = mcta(Vector2*, count, sizeof(Vector2));
    auto *start_colors    = mcta(Color*,   count, sizeof(Color));
    auto *end_colors      = mcta(Color*,   count, sizeof(Color));
    F32* lives            = mcta(F32*,     count, sizeof(F32));
    F32* sizes            = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices  = mcta(U32*,     count, sizeof(U32));
    F32* gravities        = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds  = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances  = mcta(F32*,     count, sizeof(F32));

    for (SZ i = 0; i < count; ++i) {
        F32 const flicker_x    = random_f32(-120.0F, 120.0F);
        F32 const rise_speed   = random_f32(-280.0F, -60.0F);
        F32 const heat_shimmer = random_f32(-60.0F, 60.0F);
        F32 const intensity    = random_f32(0.6F, 1.8F);

        // Create more natural fire base distribution
        F32 const base_bias = random_f32(0.0F, 1.0F);
        F32 const y_spawn   = base_bias < 0.7F ?
            spawn_rect.y + (spawn_rect.height * random_f32(0.6F, 1.0F)) : // Most flames from bottom
            spawn_rect.y + random_f32(0.0F, spawn_rect.height);           // Some from middle

        positions[i] = {
            spawn_rect.x + random_f32(0.0F, spawn_rect.width),
            y_spawn
        };

        velocities[i] = {
            (flicker_x + heat_shimmer) * intensity,
            rise_speed * intensity
        };

        start_colors[i]    = color_variation(start_color, 18);
        end_colors[i]      = color_variation(end_color, 18);
        lives[i]           = random_f32(0.25F, 1.0F);
        sizes[i]           = ui_scale_y(random_f32(0.375F, 0.75F)) * size_multiplier * intensity;
        gravities[i]       = random_f32(-80.0F, -20.0F);  // Fire rises (negative gravity)
        rotation_speeds[i] = random_f32(-2.0F, 2.0F);     // Flickering rotation
        air_resistances[i] = random_f32(0.01F, 0.025F);   // Light air resistance

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles2d_register_texture(texture);
    }

    particles2d_add(positions, velocities, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, count);
}

void particles2d_add_effect_spiral(Rectangle spawn_rect, Color start_color, Color end_color, F32 size_multiplier, SZ count) {
    // Use magical and mystical textures for spiral effect
    C8 const* magic_texture_names[] = {
        "particle_magic_01.png",
        "particle_magic_03.png",
        "particle_magic_04.png",
        "particle_magic_05.png",
        "particle_symbol_01.png",
        "particle_symbol_02.png",
        "particle_twirl_01.png",
        "particle_twirl_02.png",
        "particle_twirl_03.png"
    };

    SZ const texture_count = sizeof(magic_texture_names) / sizeof(magic_texture_names[0]);

    // Pre-load all textures into transient memory array
    auto *textures = mcta(ATexture**, texture_count, sizeof(ATexture*));
    for (SZ i = 0; i < texture_count; ++i) {
        textures[i] = asset_get_texture(magic_texture_names[i]);
    }

    auto *positions       = mcta(Vector2*, count, sizeof(Vector2));
    auto *velocities      = mcta(Vector2*, count, sizeof(Vector2));
    auto *start_colors    = mcta(Color*,   count, sizeof(Color));
    auto *end_colors      = mcta(Color*,   count, sizeof(Color));
    F32* lives            = mcta(F32*,     count, sizeof(F32));
    F32* sizes            = mcta(F32*,     count, sizeof(F32));
    U32* texture_indices  = mcta(U32*,     count, sizeof(U32));
    F32* gravities        = mcta(F32*,     count, sizeof(F32));
    F32* rotation_speeds  = mcta(F32*,     count, sizeof(F32));
    F32* air_resistances  = mcta(F32*,     count, sizeof(F32));

    Vector2 const rect_center = {spawn_rect.x + (spawn_rect.width / 2.0F), spawn_rect.y + (spawn_rect.height / 2.0F)};

    for (SZ i = 0; i < count; ++i) {
        // Spawn from center with slight randomness
        F32 const spawn_offset = random_f32(0.0F, 5.0F);
        F32 const spawn_angle  = random_f32(0.0F, 2.0F * glm::pi<F32>());

        positions[i] = {
            rect_center.x + (math_cos_f32(spawn_angle) * spawn_offset),
            rect_center.y + (math_sin_f32(spawn_angle) * spawn_offset)
        };

        // Random angle for spiral direction
        F32 const angle = random_f32(0.0F, 2.0F * glm::pi<F32>());
        F32 const speed = random_f32(100.0F, 300.0F);
        F32 const magic_intensity = random_f32(0.7F, 1.5F);

        // Velocity creates the spiral via rotation in compute shader
        velocities[i] = {
            math_cos_f32(angle) * speed * magic_intensity,
            math_sin_f32(angle) * speed * magic_intensity
        };

        start_colors[i]    = color_variation(start_color, 20);
        end_colors[i]      = color_variation(end_color, 20);
        lives[i]           = random_f32(1.5F, 5.0F);
        sizes[i]           = ui_scale_y(random_f32(0.5F, 1.0F)) * size_multiplier * magic_intensity;
        gravities[i]       = random_f32(20.0F, 100.0F);   // Magical floating effect
        rotation_speeds[i] = random_f32(-5.0F, 5.0F);     // Fast magical rotation
        air_resistances[i] = random_f32(0.008F, 0.02F);   // Mystical air resistance

        ATexture* texture  = textures[random_s32(0, texture_count - 1)];
        texture_indices[i] = particles2d_register_texture(texture);
    }

    particles2d_add(positions, velocities, sizes, start_colors, end_colors, lives, texture_indices, gravities, rotation_speeds, air_resistances, count);
}

#else

void particles2d_init() { llw("Particle2D is not supported on macOS!"); }
void particles2d_clear() {}
void particles2d_clear_scene(SceneType scene_type) {}
void particles2d_update(F32 dt) {}
void particles2d_draw() {}
U32 particles2d_register_texture(ATexture *texture) {return 0;}
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
                   SZ count) {}
void particles2d_add_effect_explosion(Rectangle spawn_rect, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles2d_add_effect_smoke    (Rectangle spawn_rect, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles2d_add_effect_sparkle  (Rectangle spawn_rect, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles2d_add_effect_fire     (Rectangle spawn_rect, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}
void particles2d_add_effect_spiral   (Rectangle spawn_rect, Color start_color, Color end_color, F32 size_multiplier, SZ count) {}

#endif
