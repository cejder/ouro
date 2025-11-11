#pragma once

#include "common.hpp"

#include <raylib.h>

// Selection indicator spawning rate (particles per second)
// Lower rate for large stationary spinning circles
#define EDIT_SELECTION_INDICATOR_PARTICLES_PER_SECOND 5.0F
#define EDIT_SELECTION_INDICATOR_PARTICLE_COUNT 5

struct EditState {
    Vector3 mouse_click_location;
    F32 selection_indicator_timer;

    // Rectangle selection state
    BOOL is_selecting_rect;
    Vector2 selection_rect_start;
    Vector2 selection_rect_end;
};

void edit_update(F32 dt, F32 dtu);
void edit_draw_2d_hud();
Vector3 edit_get_last_click_world_position();
