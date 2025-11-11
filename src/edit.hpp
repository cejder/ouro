#pragma once

#include "common.hpp"

#include <raylib.h>

struct EditState {
    Vector3 mouse_click_location;

    // Rectangle selection state
    BOOL is_selecting_rect;
    Vector2 selection_rect_start;
    Vector2 selection_rect_end;
};

void edit_update(F32 dt, F32 dtu);
void edit_draw_2d_hud();
Vector3 edit_get_last_click_world_position();
