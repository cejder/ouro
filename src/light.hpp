#pragma once

// NOTE: Light and Lighting structures have been moved to world.hpp
// This header is kept for backward compatibility but all lighting
// functions now operate on g_world->lighting

#include "common.hpp"
#include <raylib.h>

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
