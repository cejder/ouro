#pragma once

#include "common.hpp"

#include <raylib.h>

#define LIGHTS_MAX 10
#define LIGHT_DEFAULT_INTENSITY 2000.0F

// Light Types:
// - POINT: Emits light omnidirectionally from a position (like a light bulb)
// - SPOT: Cone of light from a position in a direction (like a flashlight)
enum LightType : U8 {
    LIGHT_TYPE_POINT = 0,
    LIGHT_TYPE_SPOT = 1,
};

fwd_decl(AShader);
fwd_decl(AFont);
fwd_decl(ATexture);

struct Light {
    BOOL dirty;
    BOOL in_frustum;

    S32 enabled;
    LightType type;
    Vector3 position;
    Vector3 direction;
    F32 color[4];
    F32 intensity;
    F32 inner_cutoff;
    F32 outer_cutoff;

    S32 enabled_loc;
    S32 type_loc;
    S32 position_loc;
    S32 direction_loc;
    S32 intensity_loc;
    S32 color_loc;
    S32 inner_cutoff_loc;
    S32 outer_cutoff_loc;
};

struct Lighting {
    BOOL initialized;
    Light lights[LIGHTS_MAX];
    AShader *model_shader;
    S32 model_shader_view_pos_loc;
    S32 model_shader_ambient_color_loc;
};

Lighting extern g_lighting;

void lighting_init();
void lighting_disable_all_lights();
void lighting_set_point_light(SZ idx, BOOL enabled, Vector3 position, Color color, F32 intensity);
void lighting_set_spot_light(SZ idx, BOOL enabled, Vector3 position, Vector3 direction, Color color, F32 intensity, F32 inner_cutoff, F32 outer_cutoff);
void lighting_set_light_enabled(SZ idx, BOOL enabled);
void lighting_set_light_position(SZ idx, Vector3 position);
void lighting_default_lights_setup();
void lighting_update(Camera3D *camera);
void lighting_draw_2d_dbg();
void lighting_draw_3d_dbg();
void lighting_dump();
