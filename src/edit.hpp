#pragma once

#include "common.hpp"

#include <raylib.h>

// Selection indicator spawning rate (particles per second)
#define EDIT_SELECTION_INDICATOR_PARTICLES_PER_SECOND 60.0F
#define EDIT_SELECTION_INDICATOR_PARTICLE_COUNT 12

struct EditState {
    Vector3 mouse_click_location;
    F32 selection_indicator_timer;
};

void edit_update(F32 dt, F32 dtu);
Vector3 edit_get_last_click_world_position();
