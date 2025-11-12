#include "render_healthbar.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "cvar.hpp"
#include "log.hpp"
#include "render.hpp"
#include "std.hpp"
#include "time.hpp"

#include <external/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <raymath.h>

RenderHealthbar g_render_healthbar = {};

#ifndef __APPLE__

void render_healthbar_init() {
    // Shader is already initialized in render_init()

    // Base quad vertices for instanced rendering (0-1 range)
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
    glGenVertexArrays(1, &g_render_healthbar.vao);
    glGenBuffers(1, &quad_vbo);

    glBindVertexArray(g_render_healthbar.vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(F32), (void *)nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(F32), (void *)(2 * sizeof(F32)));

    // Create healthbar SSBO with persistent mapping
    glGenBuffers(1, &g_render_healthbar.ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_render_healthbar.ssbo);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(HEALTHBAR_MAX * sizeof(HealthbarInstance)), nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    g_render_healthbar.mapped_data = (HealthbarInstance *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)(HEALTHBAR_MAX * sizeof(HealthbarInstance)), GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    if (!g_render_healthbar.mapped_data) {
        lle("Failed to map healthbar buffer!");
        return;
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Initialize mutex for thread-safe adds
    if (mtx_init(&g_render_healthbar.count_mutex, mtx_plain) != thrd_success) {
        lle("Failed to initialize healthbar mutex!");
        return;
    }

    // Clear initial state
    render_healthbar_clear();

    g_render_healthbar.initialized = true;
    lli("Healthbar rendering system initialized");
}

void render_healthbar_clear() {
    g_render_healthbar.count = 0;
}

void render_healthbar_add(Vector2 screen_pos, Vector2 bar_size, F32 health_perc) {
    // Thread-safe: acquire next slot atomically
    mtx_lock(&g_render_healthbar.count_mutex);

    if (g_render_healthbar.count >= HEALTHBAR_MAX) {
        mtx_unlock(&g_render_healthbar.count_mutex);
        llw("Healthbar buffer full! Max: %d", HEALTHBAR_MAX);
        return;
    }

    SZ const slot = g_render_healthbar.count++;
    mtx_unlock(&g_render_healthbar.count_mutex);

    // Write data outside critical section (no contention)
    HealthbarInstance *hb = &g_render_healthbar.mapped_data[slot];
    hb->screen_pos = screen_pos;
    hb->bar_size = bar_size;
    hb->health_perc = health_perc;
    hb->padding[0] = 0.0F;
    hb->padding[1] = 0.0F;
    hb->padding[2] = 0.0F;
}

void render_healthbar_draw() {
    if (!g_render_healthbar.initialized) { return; }
    if (g_render_healthbar.count == 0) { return; }

    RenderHealthbarShader const *shader = &g_render.healthbar_shader;
    BeginShaderMode(shader->shader->base);

    // Get render resolution for screen projection
    Vector2 const render_res = render_get_render_resolution();

    // Screen-space orthographic projection matrix
    Matrix const projection = MatrixOrtho(0.0F, render_res.x, render_res.y, 0.0F, -1.0F, 1.0F);
    SetShaderValueMatrix(shader->shader->base, shader->view_proj_loc, projection);

    // Set time for pulsing animation
    F32 const current_time = time_get();
    SetShaderValue(shader->shader->base, shader->time_loc, &current_time, SHADER_UNIFORM_FLOAT);

    // Set healthbar count
    U32 const count = (U32)g_render_healthbar.count;
    SetShaderValue(shader->shader->base, shader->healthbar_count_loc, &count, SHADER_UNIFORM_UINT);

    // Set background color (dark semi-transparent)
    F32 const bg_color[4] = {30.0F / 255.0F, 30.0F / 255.0F, 30.0F / 255.0F, 0.88F};
    SetShaderValue(shader->shader->base, shader->bg_color_loc, bg_color, SHADER_UNIFORM_VEC4);

    // Set border color (beige semi-transparent)
    F32 const border_color[4] = {245.0F / 255.0F, 245.0F / 255.0F, 220.0F / 255.0F, 0.20F};
    SetShaderValue(shader->shader->base, shader->border_color_loc, border_color, SHADER_UNIFORM_VEC4);

    // Set border thickness
    F32 const border_thickness = ui_scale_x(0.20F);
    SetShaderValue(shader->shader->base, shader->border_thickness_loc, &border_thickness, SHADER_UNIFORM_FLOAT);

    // Bind healthbar data SSBO (binding point 4)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, g_render_healthbar.ssbo);

    // Draw all healthbars using instancing
    glBindVertexArray(g_render_healthbar.vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (S32)g_render_healthbar.count);

    glBindVertexArray(0);

    EndShaderMode();

    INCREMENT_DRAW_CALL;
}

#else

void render_healthbar_init() { llw("Healthbar rendering is not supported on macOS!"); }
void render_healthbar_clear() {}
void render_healthbar_add(Vector2 screen_pos, Vector2 bar_size, F32 health_perc) {}
void render_healthbar_draw() {}

#endif
