#pragma once

#include "common.hpp"
#include "world.hpp"

#include <raylib.h>

fwd_decl(AShader);

#define HEALTHBAR_MAX WORLD_MAX_ENTITIES

// GPU-aligned healthbar instance structure (must match shader)
// Total size: 32 bytes (multiple of 16)
struct HealthbarInstance {
    Vector3 world_pos;      // 12 bytes - world position
    F32 entity_radius;      // 4 bytes - entity radius for size calculation
    F32 health_perc;        // 4 bytes - health percentage (0.0 - 1.0)
    F32 padding[3];         // 12 bytes - padding to align to 32 bytes
};

struct RenderHealthbar {
    BOOL initialized;

    // Rendering
    U32 vao;

    // GPU data
    U32 ssbo;
    HealthbarInstance *mapped_data;

    // State
    SZ count;  // Current number of healthbars to draw this frame
};

extern RenderHealthbar g_render_healthbar;

void render_healthbar_init();
void render_healthbar_clear();
void render_healthbar_add(Vector3 world_pos, F32 entity_radius, F32 health_perc);
void render_healthbar_draw();
