#include "core.hpp"
#include "arg.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "console.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "input.hpp"
#include "log.hpp"
#include "math.hpp"
#include "memory.hpp"
#include "message.hpp"
#include "option.hpp"
#include "particles_2d.hpp"
#include "particles_3d.hpp"
#include "profiler.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "string.hpp"
#include "test.hpp"
#include "time.hpp"
#include "world.hpp"

#include <external/glfw/include/GLFW/glfw3.h>

Core g_core = {};

void static i_exit_if_not_initialized() {
    if (!g_core.initialized) {
        llw("Core not initialized, exiting.");
        exit(EXIT_FAILURE);
    }
}

void static i_set_cursor() {
    ATexture *cursor_texture = asset_get_texture("cursor_default.png");
    GLFWimage cursor_image;
    cursor_image.width  = cursor_texture->image.width;
    cursor_image.height = cursor_texture->image.height;
    cursor_image.pixels = (U8 *)cursor_texture->image.data;
    GLFWcursor *cursor  = glfwCreateCursor(&cursor_image, 0, 0);
    glfwSetCursor(glfwGetCurrentContext(), cursor);
}

void static i_set_icon() {
    ATexture *icon_texture = asset_get_texture("icon.png");
    SetWindowIcon(icon_texture->image);
    // TODO: void SetWindowIcons(Image *images, int count)
}

void core_init(U8 major, U8 minor, U8 patch, C8 const *build_type) {
    g_core.running        = true;
    g_core.version_info   = info_create_version_info(major, minor, patch, build_type);
    g_core.cpu_core_count = info_get_cpu_core_count();

    SetRandomSeed((U32)time(nullptr));
    SetTraceLogCallback((TraceLogCallback)llog_raylib_cb);
    LLogLevel const rl_level = llog_get_level();
    SetTraceLogLevel(rl_level);

    U32 flags = FLAG_WINDOW_RESIZABLE;
#if defined(__APPLE__)
    flags |= FLAG_WINDOW_HIGHDPI | FLAG_FULLSCREEN_MODE
#endif
    if (c_video__vsync) { flags |= FLAG_VSYNC_HINT; }
    SetConfigFlags(flags);
    SetTargetFPS(c_video__fps_max);

    // When the arguments claim that we are in a debugger build
    // (not to be confused with a debug build), we will set the
    // appropriate title our scripts expect.
    String *ouro_title = args_get_bool("InDebugger") ? PS("%s_debug", OURO_TITLE) :  PS("%s %s", OURO_TITLE, g_core.version_info.c);

    InitWindow(c_video__window_resolution_width, c_video__window_resolution_height, ouro_title->c);
    SetExitKey(0);

    option_init();
    render_init();
    math_init();
    time_init();

    loading_prepare(&g_core.loading, 8);  // WARN: This needs to be manually adjusted if we get more steps. Otherwise they won't be tracked.

    i_set_cursor();
    i_set_icon();

    // TODO: Does the order matter? It probably does. Do we have the best order?
    // Probably not. Why am I the best looking dude on this side of the rhine?
    // Nobody will ever know. But I digress.
    // Let's continue with the initialization. Eat my anus.

    SceneType const start_scene = OURO_IS_DEBUG ? SCENE_OVERWORLD : SCENE_OVERWORLD;

    LD_TRACK(&g_core.loading, "Initializing \\ouc{#b3ffb3ff}Profiler",      profiler_init());
    LD_TRACK(&g_core.loading, "Initializing \\ouc{#b3ffb3ff}Audio Manager", audio_init());
    LD_TRACK(&g_core.loading, "Initializing \\ouc{#b3ffb3ff}Input Manager", input_init());
    LD_TRACK(&g_core.loading, "Initializing \\ouc{#b3ffb3ff}Debug Manager", dbg_init());
    LD_TRACK(&g_core.loading, "Initializing \\ouc{#b3ffb3ff}Asset Manager", asset_init());
    LD_TRACK(&g_core.loading, "Initializing \\ouc{#b3ffb3ff}World",         world_init());
    LD_TRACK(&g_core.loading, "Initializing \\ouc{#b3ffb3ff}Scenes",        scenes_init());
    LD_TRACK(&g_core.loading, TS("Entering \\ouc{#b3ffb3ff}Scene: \\ouc{#9db4c0ff}%s", scenes_to_str(start_scene)->c)->c, scenes_set_scene(start_scene));

    if (g_options.fresh_install) { option_set_optimal_settings(); };

    loading_finish(&g_core.loading);

    if (OURO_IS_DEBUG) { info_print_system(&g_core.version_info); }

    g_core.initialized = true;


    // HACK: Check if we want the Steam Deck or macOS config
#if defined(__APPLE__)
    option_set_video_resolution({2560, 1664});
    option_set_video_max_fps(60);
    option_set_video_vsync(true);
#else
    BOOL const on_deck = args_get_bool("SteamDeck");
    if (on_deck) {
        option_set_video_resolution({1280, 800});
        option_set_video_max_fps(0);
        option_set_video_vsync(false);
    }
#endif
}

void core_quit() {
    i_exit_if_not_initialized();

    dbg_quit();
    CloseWindow();
    audio_quit();
    asset_quit();
    cvar_save();
}

void core_error_quit() {
    llw("Exiting due to error");
    exit(EXIT_FAILURE);
}

void core_run() {
    i_exit_if_not_initialized();

    asset_start_reload_thread();

    if (args_get_bool("JustTest")) {
        if (!test_run()) { core_error_quit(); }
        lli("Exiting after tests.");
        return;
    }

    while (g_core.running) {
        if (WindowShouldClose()) { g_core.running = false; }
        ML_PROFILE(core_loop());
        PP(profiler_finalize());
    }
}

void core_loop() {
    PP(core_update());
    PP(core_draw());
    PP(core_post());
}

void core_update() {
    F32 const dt = time_get_delta();
    F32 const dtu = time_get_delta_untouched();

    PP(time_update(dt, dtu));
    PP(profiler_update());
    PP(option_update());
    PP(input_update());
    PP(asset_update());
    PP(audio_update(dt));
    PP(color_update(dtu));
    PP(render_update(dt));
    PP(particles2d_update(dt));
    PP(particles3d_update(dt));
    PP(math_update());
    PP(scenes_update(dt, dtu));
    PP(messages_update(dtu));
    PP(dbg_update());
}

void core_draw() {
    PP(render_begin());
    {
        PP(scenes_draw());
        RMODE_BEGIN(RMODE_LAST_LAYER) {
            PP(input_draw());
            PP(profiler_draw());
            PP(messages_draw());
            PP(dbg_draw());
        }
        RMODE_END;
    }
    PP(render_end());
}

void core_post() {
    PP(render_post());
    PP(memory_post());
}
