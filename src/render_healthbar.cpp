#include "render_healthbar.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "cvar.hpp"
#include "log.hpp"
#include "render.hpp"
#include "std.hpp"

#include <external/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <raymath.h>

RenderHealthbar g_render_healthbar = {};

void render_healthbar_init() {
    // Load shader and cache uniform locations
    g_render_healthbar.shader.shader = asset_get_shader("healthbar");

    g_render_healthbar.shader.projection_loc = GetShaderLocation(g_render_healthbar.shader.shader->base, "u_projection");
    g_render_healthbar.shader.healthbar_count_loc = GetShaderLocation(g_render_healthbar.shader.shader->base, "u_healthbar_count");
    g_render_healthbar.shader.bg_color_loc = GetShaderLocation(g_render_healthbar.shader.shader->base, "u_bg_color");
    g_render_healthbar.shader.border_color_loc = GetShaderLocation(g_render_healthbar.shader.shader->base, "u_border_color");
    g_render_healthbar.shader.border_thickness_loc = GetShaderLocation(g_render_healthbar.shader.shader->base, "u_border_thickness");

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

    // Clear initial state
    render_healthbar_clear();

    g_render_healthbar.initialized = true;
    lli("Healthbar rendering system initialized");
}

void render_healthbar_clear() {
    g_render_healthbar.count = 0;
}

void render_healthbar_add(Vector2 screen_pos, Vector2 size, Color fill_color, F32 health_perc, F32 roundness, BOOL is_multi_select) {
    if (g_render_healthbar.count >= HEALTHBAR_MAX) {
        llw("Healthbar buffer full! Max: %d", HEALTHBAR_MAX);
        return;
    }

    HealthbarInstance *hb = &g_render_healthbar.mapped_data[g_render_healthbar.count];
    hb->screen_pos = screen_pos;
    hb->size = size;

    // Convert Color (4 bytes) to vec4 (16 bytes) for GPU
    hb->fill_color[0] = (F32)fill_color.r / 255.0F;
    hb->fill_color[1] = (F32)fill_color.g / 255.0F;
    hb->fill_color[2] = (F32)fill_color.b / 255.0F;
    hb->fill_color[3] = (F32)fill_color.a / 255.0F;

    hb->health_perc = health_perc;
    hb->roundness = roundness;
    hb->is_multi_select = is_multi_select ? 1U : 0U;
    hb->padding = 0U;

    g_render_healthbar.count++;
}

void render_healthbar_draw() {
    if (!g_render_healthbar.initialized) { return; }
    if (g_render_healthbar.count == 0) { return; }

    BeginShaderMode(g_render_healthbar.shader.shader->base);

    // Set projection matrix
    Vector2 const render_res = render_get_render_resolution();
    Matrix const projection = MatrixOrtho(0.0F, render_res.x, render_res.y, 0.0F, -1.0F, 1.0F);
    SetShaderValueMatrix(g_render_healthbar.shader.shader->base, g_render_healthbar.shader.projection_loc, projection);

    // Set healthbar count
    U32 const count = (U32)g_render_healthbar.count;
    SetShaderValue(g_render_healthbar.shader.shader->base, g_render_healthbar.shader.healthbar_count_loc, &count, SHADER_UNIFORM_UINT);

    // Set background color (dark semi-transparent)
    F32 const bg_color[4] = {30.0F / 255.0F, 30.0F / 255.0F, 30.0F / 255.0F, 0.88F};
    SetShaderValue(g_render_healthbar.shader.shader->base, g_render_healthbar.shader.bg_color_loc, bg_color, SHADER_UNIFORM_VEC4);

    // Set border color (beige semi-transparent)
    F32 const border_color[4] = {245.0F / 255.0F, 245.0F / 255.0F, 220.0F / 255.0F, 0.20F};
    SetShaderValue(g_render_healthbar.shader.shader->base, g_render_healthbar.shader.border_color_loc, border_color, SHADER_UNIFORM_VEC4);

    // Set border thickness
    F32 const border_thickness = ui_scale_x(0.20F);
    SetShaderValue(g_render_healthbar.shader.shader->base, g_render_healthbar.shader.border_thickness_loc, &border_thickness, SHADER_UNIFORM_FLOAT);

    // Bind healthbar data SSBO (binding point 4)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, g_render_healthbar.ssbo);

    // Draw all healthbars using instancing
    glBindVertexArray(g_render_healthbar.vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (S32)g_render_healthbar.count);

    glBindVertexArray(0);

    EndShaderMode();

    INCREMENT_DRAW_CALL;
}
