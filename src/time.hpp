#pragma once

#include "common.hpp"

#define TIME_MAX_DELTA_TIME (1.0F / 10.0F)
#define TIME_FRAME_TIMES_TIMELINE_MAX_COUNT 256

struct Time {
    F32 delta_mod;
    F64 time;
    F64 time_untouched;
    F32 timeline_frame_times[TIME_FRAME_TIMES_TIMELINE_MAX_COUNT];
};

void time_init();
void time_update(F32 dt, F32 dtu);
void time_reset();
void time_set_delta_mod(F32 delta_mod);
F32 time_get_delta_mod();
F32 *time_get_delta_mod_ptr();
F32 time_get_delta();
F32 time_get_delta_untouched();
F32 time_get();
F32 time_get_untouched();
F32 time_get_glfw();
F64 time_get_glfw_f64();
F32 *time_get_frame_times();
BOOL time_is_paused();
