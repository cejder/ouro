#pragma once

#include "common.hpp"
#include "world.hpp"

#include <raylib.h>
#include <tinycthread.h>

// Need for tinycthread on macOS
#ifdef call_once
#undef call_once
#endif

fwd_decl(AShader);

#define HEALTHBAR_MAX WORLD_MAX_ENTITIES

// GPU-aligned healthbar instance structure (must match shader)
// Total size: 32 bytes (multiple of 16)
struct HealthbarInstance {
    Vector2 screen_pos;     // 8 bytes - screen position (center)
    Vector2 bar_size;       // 8 bytes - bar width and height in pixels
    F32 health_perc;        // 4 bytes - health percentage (0.0 - 1.0)
    F32 padding[3];         // 12 bytes - padding to align to 32 bytes
};

struct RenderHealthbar {
    U32 vao;

    U32 ssbo;
    HealthbarInstance *mapped_data;

    SZ count;           // Current number of healthbars to draw this frame
    mtx_t count_mutex;  // Protects count for multithreaded adds
};

RenderHealthbar extern g_render_healthbar;

void render_healthbar_init();
void render_healthbar_clear();
void render_healthbar_add(Vector2 screen_pos, Vector2 bar_size, F32 health_perc);
void render_healthbar_draw();
