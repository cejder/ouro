#pragma once

#include "common.hpp"
#include "math.hpp"
#include "asset.hpp"
#include "entity.hpp"
#include "world.hpp"

#include <raylib.h>

#define SELECTION_INDICATOR_MAX_COUNT WORLD_MAX_ENTITIES
#define SELECTION_INDICATOR_ROTATION_SPEED 0.524F  // ~30 degrees/second in radians
#define SELECTION_INDICATOR_Y_OFFSET 0.15F

// GPU-aligned instance data for selection indicators (must match shader)
struct SelectionIndicatorInstanceData {
    Vector3 position;       // 12 bytes
    F32 rotation;           // 4 bytes
    F32 size;               // 4 bytes
    F32 terrain_normal_x;   // 4 bytes
    F32 terrain_normal_y;   // 4 bytes
    F32 terrain_normal_z;   // 4 bytes
    ColorF color;           // 16 bytes (vec4 in shader)
    F32 padding[2];         // 8 bytes - align to 64 bytes
};

struct SelectionIndicator {
    EID entity_id;
    F32 rotation;
    BOOL active;
};

struct SelectionIndicators {
    // Indicator data
    SelectionIndicator indicators[SELECTION_INDICATOR_MAX_COUNT];
    SZ active_count;

    // Rendering resources
    U32 vao;
    U32 vbo;  // Instance data VBO
    U32 quad_vbo;  // Quad vertex positions
    AShader* shader;
    U32 texture_index;
    ATexture* texture;

    // Cached uniform locations
    S32 view_proj_loc;
    S32 camera_pos_loc;
    S32 camera_right_loc;
    S32 camera_up_loc;
    S32 texture_loc;

    // Color configuration
    ColorF indicator_color;
};

extern SelectionIndicators g_selection_indicators;

void selection_indicators_init();
void selection_indicators_shutdown();
void selection_indicators_update(F32 dt);
void selection_indicators_draw();

// Selection management
void selection_indicators_clear();
void selection_indicators_add(EID entity_id);
void selection_indicators_remove(EID entity_id);
void selection_indicators_sync_with_world();
