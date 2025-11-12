#pragma once

#include "common.hpp"

#include <raylib.h>

fwd_decl(AShader);

#define HEALTHBAR_MAX 1000

// GPU-aligned healthbar instance structure (must match shader)
// Total size: 48 bytes (multiple of 16)
struct HealthbarInstance {
    Vector2 screen_pos;     // 8 bytes - center position on screen
    Vector2 size;           // 8 bytes - width and height
    F32 fill_color[4];      // 16 bytes - RGBA as vec4 (must match shader)
    F32 health_perc;        // 4 bytes - health percentage (0.0 - 1.0)
    F32 roundness;          // 4 bytes - corner roundness
    U32 is_multi_select;    // 4 bytes - 0 = single select (complex), 1 = multi select (minimal)
    U32 padding;            // 4 bytes - padding to align to 48 bytes
};

struct RenderHealthbarShader {
    AShader *shader;
    S32 projection_loc;
    S32 healthbar_count_loc;
    S32 bg_color_loc;
    S32 border_color_loc;
    S32 border_thickness_loc;
};

struct RenderHealthbar {
    BOOL initialized;

    // Rendering
    U32 vao;
    RenderHealthbarShader shader;

    // GPU data
    U32 ssbo;
    HealthbarInstance *mapped_data;

    // State
    SZ count;  // Current number of healthbars to draw this frame
};

extern RenderHealthbar g_render_healthbar;

void render_healthbar_init();
void render_healthbar_clear();
void render_healthbar_add(Vector2 screen_pos, Vector2 size, Color fill_color, F32 health_perc, F32 roundness, BOOL is_multi_select);
void render_healthbar_draw();
