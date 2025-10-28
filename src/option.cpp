#include "option.hpp"
#include "cvar.hpp"
#include "math.hpp"
#include "profiler.hpp"
#include "render.hpp"

#include <raymath.h>
#include <external/glfw/include/GLFW/glfw3.h>

#define OPTION_INI_FILE_PATH "options.ini"
#define OPTION_DEFAULT_SHOW_FRAME_INFO false
#define OPTION_DEFAULT_VSYNC true
#define OPTION_DEFAULT_RESOLUTION_X 1920
#define OPTION_DEFAULT_RESOLUTION_Y 1080
#define OPTION_DEFAULT_MAX_FPS 60
#define OPTION_DEFAULT_MUSIC_VOLUME 0.50F
#define OPTION_DEFAULT_SFX_VOLUME 0.50F
#define OPTION_DEFAULT_AMBIENCE_VOLUME 0.50F
#define OPTION_DEFAULT_VOICE_VOLUME 0.50F
#define OPTION_MAX_FPS_LOWEST 24
#define OPTION_MAX_FPS_HIGHEST 999
#define OPTION_MIN_RESOLUTION_X 800
#define OPTION_MIN_RESOLUTION_Y 600

Options g_options = {};

Vector2 static i_get_native_resolution() {
    S32 const current_monitor = GetCurrentMonitor();
    return {(F32)GetMonitorWidth(current_monitor), (F32)GetMonitorHeight(current_monitor)};
}

S32 static i_get_native_refresh_rate() {
    S32 const current_monitor = GetCurrentMonitor();
    return GetMonitorRefreshRate(current_monitor);
}

void static i_populate_available_resolutions() {
    array_clear(&g_options.available_resolutions);

    S32 count = 0;
    GLFWvidmode const *modes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &count);

    for (S32 i = 0; i < count; ++i) {
        Vector2 const res = {(F32)modes[i].width, (F32)modes[i].height};

        // Skip duplicates and very small resolutions
        if (res.x < OPTION_MIN_RESOLUTION_X || res.y < OPTION_MIN_RESOLUTION_Y) { continue; }

        BOOL duplicate = false;
        array_each(j, &g_options.available_resolutions) {
            if (Vector2Equals(res, array_get(&g_options.available_resolutions, j))) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) { array_push(&g_options.available_resolutions, res); }
    }

    // Sort by total pixels (width * height)
    for (SZ i = 0; i < g_options.available_resolutions.count - 1; ++i) {
        for (SZ j = 0; j < g_options.available_resolutions.count - i - 1; ++j) {
            Vector2 const a = array_get(&g_options.available_resolutions, j);
            Vector2 const b = array_get(&g_options.available_resolutions, j + 1);
            if (a.x * a.y > b.x * b.y) {
                array_set(&g_options.available_resolutions, j, b);
                array_set(&g_options.available_resolutions, j + 1, a);
            }
        }
    }
}

void option_init() {
    i_populate_available_resolutions();
}

void option_set_optimal_settings() {
    S32 const monitor_refresh_rate = i_get_native_refresh_rate();
    Vector2 const monitor_res = i_get_native_resolution();

    // Find the closest resolution to the monitor's resolution
    Vector2 best_res = {(F32)OPTION_DEFAULT_RESOLUTION_X, (F32)OPTION_DEFAULT_RESOLUTION_Y};  // fallback
    if (g_options.available_resolutions.count > 0) {
        best_res = array_get(&g_options.available_resolutions, 0);
        F32 min_diff = F32_MAX;

        array_each(i, &g_options.available_resolutions) {
            Vector2 const res = array_get(&g_options.available_resolutions, i);
            F32 const diff = math_abs_f32(res.x - monitor_res.x) + math_abs_f32(res.y - monitor_res.y);
            if (diff < min_diff) {
                min_diff = diff;
                best_res = res;
            }
        }
    }

    option_set_video_resolution(best_res);
    option_set_video_max_fps(monitor_refresh_rate);

    S32 const x = (S32)(monitor_res.x - best_res.x) / 2;
    S32 const y = (S32)(monitor_res.y - best_res.y) / 2;
    SetWindowPosition(x, y);
}

Vector2 option_get_previous_video_resolution() {
    if (g_options.available_resolutions.count == 0) { return {(F32)OPTION_DEFAULT_RESOLUTION_X, (F32)OPTION_DEFAULT_RESOLUTION_Y}; }

    SZ current_idx = SZ_MAX;
    Vector2 const res = render_get_render_resolution();

    array_each(i, &g_options.available_resolutions) {
        if (Vector2Equals(res, array_get(&g_options.available_resolutions, i))) {
            current_idx = i;
            break;
        }
    }

    if (current_idx == SZ_MAX) { return array_get(&g_options.available_resolutions, 0); }

    S32 new_idx = (S32)current_idx - 1;
    if (new_idx < 0) { new_idx = (S32)g_options.available_resolutions.count - 1; }

    return array_get(&g_options.available_resolutions, new_idx);
}

Vector2 option_get_next_video_resolution() {
    if (g_options.available_resolutions.count == 0) { return {(F32)OPTION_DEFAULT_RESOLUTION_X, (F32)OPTION_DEFAULT_RESOLUTION_Y}; }

    SZ current_idx = SZ_MAX;
    Vector2 const res = render_get_render_resolution();

    array_each(i, &g_options.available_resolutions) {
        if (Vector2Equals(res, array_get(&g_options.available_resolutions, i))) {
            current_idx = i;
            break;
        }
    }

    if (current_idx == SZ_MAX) { return array_get(&g_options.available_resolutions, 0); }

    S32 new_idx = (S32)current_idx + 1;
    if (new_idx >= (S32)g_options.available_resolutions.count) { new_idx = 0; }

    return array_get(&g_options.available_resolutions, new_idx);
}

S32 option_get_previous_video_max_fps() {
    // If we are already at lowest, we return 0 (no limit).
    if (c_video__fps_max <= OPTION_MAX_FPS_LOWEST) { return 0; }
    // If it's already higher than the highest possible value, we return the highest possible value.
    if (c_video__fps_max > OPTION_MAX_FPS_HIGHEST) { return OPTION_MAX_FPS_HIGHEST; }

    return c_video__fps_max - 1;
}

S32 option_get_next_video_max_fps() {
    // If we are currently at 0 (no limit), we return the lowest possible value.
    if (c_video__fps_max == 0) { return OPTION_MAX_FPS_LOWEST; }
    // If we are already at highest, we return the highest possible value.
    if (c_video__fps_max >= OPTION_MAX_FPS_HIGHEST) { return OPTION_MAX_FPS_HIGHEST; }

    return c_video__fps_max + 1;
}

BOOL option_get_audio_enabled(AudioChannelGroup channel_group) {
    return *audio_get_volume_ptr(channel_group) > 0.0F;
}

void option_set_game_show_frame_info(BOOL frame_info) {
    c_video__fps_info = frame_info;
    profiler_reset();
}

void option_set_video_resolution(Vector2 res) {
    c_video__window_resolution_width  = (S32)res.x;
    c_video__window_resolution_height = (S32)res.y;
    render_update_render_resolution(res);
    S32 const window_pos_x = (GetMonitorWidth(GetCurrentMonitor()) / 2) - ((S32)res.x / 2);
    S32 const window_pos_y = (GetMonitorHeight(GetCurrentMonitor()) / 2) - ((S32)res.y / 2);
    SetWindowSize(S32(res.x), S32(res.y));
    SetWindowPosition(window_pos_x, window_pos_y);
}

void option_set_video_max_fps(S32 fps) {
    c_video__fps_max = fps;
    SetTargetFPS(fps);
}

void option_set_video_vsync(BOOL vsync) {
    c_video__vsync = vsync;
    glfwSwapInterval(vsync);
}

void option_set_audio_enabled(AudioChannelGroup channel_group, BOOL enabled) {
    F32 *volume = audio_get_volume_ptr(channel_group);
    if (enabled) {
        *volume = AUDIO_VOLUME_DEFAULT_VALUE;
    } else {
        *volume = 0.0F;
    }
}

void option_set_audio_volume(AudioChannelGroup channel_group, F32 volume) {
    audio_set_volume(channel_group, volume);
}

void option_update() {
    S32 static prev_resolution_width  = 0;
    S32 static prev_resolution_height = 0;
    S32 static prev_fps_max           = 0;
    BOOL static prev_vsync            = false;
    BOOL static prev_fps_info         = false;

    if (c_video__window_resolution_width  != prev_resolution_width ||
        c_video__window_resolution_height != prev_resolution_height) {
        option_set_video_resolution({(F32)c_video__window_resolution_width, (F32)c_video__window_resolution_height});
        prev_resolution_width  = c_video__window_resolution_width;
        prev_resolution_height = c_video__window_resolution_height;
    }

    if (c_video__fps_max != prev_fps_max) {
        option_set_video_max_fps(c_video__fps_max);
        prev_fps_max = c_video__fps_max;
    }

    if (c_video__vsync != prev_vsync) {
        option_set_video_vsync(c_video__vsync);
        prev_vsync = c_video__vsync;
    }

    if (c_video__fps_info != prev_fps_info) {
        option_set_game_show_frame_info(c_video__fps_info);
        prev_fps_info = c_video__fps_info;
    }
}
