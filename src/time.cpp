#include "time.hpp"
#include "unit.hpp"

#include <raylib.h>
#include <glm/common.hpp>
#include <external/glfw/include/GLFW/glfw3.h>

Time static i_time = {};

void time_init() {
    i_time.delta_mod = 1.0F;
}

void time_update(F32 dt, F32 dtu) {
    F32 const dt_ms = (F32)BASE_TO_MILLI(time_get_delta_untouched());
    i_time.timeline_frame_times[TIME_FRAME_TIMES_TIMELINE_MAX_COUNT - 1] = dt_ms;
    i_time.time           += dt;
    i_time.time_untouched += dtu;

    for (SZ i = 0; i < TIME_FRAME_TIMES_TIMELINE_MAX_COUNT - 1; ++i) { i_time.timeline_frame_times[i] = i_time.timeline_frame_times[i + 1]; }
}

void time_reset() {
    for (F32 &frame_time : i_time.timeline_frame_times) { frame_time = 0.0F; }
}

void time_set_delta_mod(F32 delta_mod) {
    i_time.delta_mod = delta_mod;
}

F32 time_get_delta_mod() {
    return i_time.delta_mod;
}

F32 *time_get_delta_mod_ptr() {
    return &i_time.delta_mod;
}

F32 time_get_delta() {
    F32 const dt_raw = time_get_delta_untouched();
    F32 dt           = dt_raw * i_time.delta_mod;

    // Scale max delta with delta mod, but cap the scaling
    F32 max_delta = TIME_MAX_DELTA_TIME;
    if (i_time.delta_mod > 1.0F) {
        // Allow longer timesteps when speeding up, but not unlimited
        F32 const max_mod_scaling = 5.0F;  // Don't allow more than 5x base max delta
        F32 const effective_mod   = glm::min(i_time.delta_mod, max_mod_scaling);
        max_delta                *= effective_mod;
    }

    dt = glm::min(dt, max_delta);

    return dt;
}

F32 time_get_delta_untouched() {
    return GetFrameTime();
}

F32 time_get() {
    return (F32)(i_time.time);
}

F32 time_get_untouched() {
    return (F32)(i_time.time_untouched);
}

F32 time_get_glfw() {
    return (F32)glfwGetTime();
}

F64 time_get_glfw_f64() {
    return glfwGetTime();
}

F32 *time_get_frame_times() {
    return i_time.timeline_frame_times;
}

BOOL time_is_paused() {
    return i_time.delta_mod == 0.0F;
}
