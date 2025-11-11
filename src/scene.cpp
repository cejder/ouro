#include "scene.hpp"
#include "assert.hpp"
#include "log.hpp"
#include "player.hpp"
#include "profiler.hpp"
#include "render.hpp"
#include "std.hpp"
#include "string.hpp"
#include "time.hpp"

Scenes g_scenes = {};

#define MAX_SCENE_NAME_LENGTH 128

#define SCENE(NAME, _) #NAME,
C8 static const *i_scene_type_strings[] = {"NONE", LIST_OF_SCENES "COUNT"};
#undef SCENE

void static i_exit_if_some(BOOL overlay) {
    SceneType current_scene_type = SCENE_NONE;

    if (!overlay) {
        current_scene_type = g_scenes.current_scene_type;
        // Exit early if we are not in a scene.
        if (current_scene_type == SCENE_NONE) { return; }
    } else {
        current_scene_type = g_scenes.current_overlay_scene_type;
    }

    if (current_scene_type != SCENE_NONE) { g_scenes.scenes[current_scene_type].exit(); }

    profiler_reset();
}

void scenes_init() {
    g_scenes.initialized = true;

#define SCENE(NAME, name)                                                   \
    g_scenes.scenes[SCENE_##NAME].type        = SCENE_##NAME;               \
    g_scenes.scenes[SCENE_##NAME].init        = scene_##name##_init;        \
    g_scenes.scenes[SCENE_##NAME].enter       = scene_##name##_enter;       \
    g_scenes.scenes[SCENE_##NAME].resume      = scene_##name##_resume;      \
    g_scenes.scenes[SCENE_##NAME].exit        = scene_##name##_exit;        \
    g_scenes.scenes[SCENE_##NAME].update      = scene_##name##_update;      \
    g_scenes.scenes[SCENE_##NAME].draw        = scene_##name##_draw;        \
    g_scenes.scenes[SCENE_##NAME].clear_color = SCENE_DEFAULT_CLEAR_COLOR;  \
    if (!OURO_IS_DEBUG) {                                                   \
        g_scenes.scenes[SCENE_##NAME].init(&g_scenes.scenes[SCENE_##NAME]); \
        g_scenes.scenes[SCENE_##NAME].initialized = true;                   \
    }
    LIST_OF_SCENES  // NOLINT
#undef SCENE

}

void scenes_set_scene(SceneType scene_type) {
    Scene *s = &g_scenes.scenes[scene_type];
    if (!g_scenes.initialized_scene[scene_type]) {
        if (!s->initialized) { s->init(s); }
        g_scenes.initialized_scene[scene_type] = true;
    }
    g_scenes.next_scene_type = scene_type;
    i_exit_if_some(false);
    s->enter();
}

SceneType scenes_set_scene_by_name(C8 *scene_name) {
    for (S32 i = 0; i < SCENE_COUNT; ++i) {
        C8 lower_scene_type_name[MAX_SCENE_NAME_LENGTH] = {};
        ou_strncpy(lower_scene_type_name, i_scene_type_strings[i], MAX_SCENE_NAME_LENGTH);
        ou_to_lower(lower_scene_type_name);
        ou_to_lower(scene_name);

        if (ou_strcmp(scene_name, lower_scene_type_name) == 0) {
            lli("Changing scene to %s", i_scene_type_strings[i]);

            // If there is an overlay screen, we need to drop it.
            // This might cause issues, but we will see.
            g_scenes.current_overlay_scene_type = SCENE_NONE;

            scenes_set_scene((SceneType)i);
            return (SceneType)i;
        }
    }

    llw("Could not find scene with name %s", scene_name);

    return SCENE_NONE;
}

void scenes_change_overlay_scene(SceneType scene_type) {
    _assert_(g_scenes.current_overlay_scene_type != SCENE_NONE, "Overlay scene should have existed here");
    g_scenes.next_overlay_scene_type = scene_type;
    i_exit_if_some(true);
}

void scenes_push_overlay_scene(SceneType scene_type) {
    Scene *s = &g_scenes.scenes[scene_type];
    if (!g_scenes.initialized_scene[scene_type]) {
        s->init(s);
        g_scenes.initialized_scene[scene_type] = true;
    }
    g_scenes.next_overlay_scene_type = scene_type;

#ifdef OURO_IS_DEBUG_NUM
    F32 const start_time = time_get_glfw();
#endif
    s->enter();
#ifdef OURO_IS_DEBUG_NUM
    lld("entering the scene %s took %.2fs", scenes_to_cstr(scene_type), time_get_glfw() - start_time);
#endif

    profiler_reset();
}

void scenes_pop_overlay_scene(BOOL resume, BOOL skip_next_draw) {
    i_exit_if_some(true);

    if (resume) { g_scenes.scenes[g_scenes.current_scene_type].resume(); }
    g_scenes.current_overlay_scene_type = SCENE_NONE;
    g_scenes.skip_draw                  = skip_next_draw;
}

void scenes_update(F32 dt, F32 dtu) {
    if (g_scenes.next_scene_type != SCENE_NONE) {
        g_scenes.current_scene_type = g_scenes.next_scene_type;
        g_scenes.next_scene_type    = SCENE_NONE;
    }

    if (g_scenes.next_overlay_scene_type != SCENE_NONE) {
        g_scenes.current_overlay_scene_type = g_scenes.next_overlay_scene_type;
        g_scenes.next_overlay_scene_type    = SCENE_NONE;
    }

    Scene *s = &g_scenes.scenes[g_scenes.current_scene_type];
    if (g_scenes.current_overlay_scene_type != SCENE_NONE) { s = &g_scenes.scenes[g_scenes.current_overlay_scene_type]; }

    s->update(dt, dtu);
}

void scenes_draw() {
    if (g_scenes.skip_draw) {
        g_scenes.skip_draw = false;
        return;
    }

    Scene *s = &g_scenes.scenes[g_scenes.current_scene_type];
    if (g_scenes.current_overlay_scene_type != SCENE_NONE) { s = &g_scenes.scenes[g_scenes.current_overlay_scene_type]; }

    s->draw();
}

Color scenes_get_clear_color() {
    Scene const *s = &g_scenes.scenes[g_scenes.current_scene_type];
    if (g_scenes.current_overlay_scene_type != SCENE_NONE) { s = &g_scenes.scenes[g_scenes.current_overlay_scene_type]; }
    return s->clear_color;
}

C8 const *scenes_to_cstr(SceneType scene_type) {
    return i_scene_type_strings[scene_type];
}

C8 const *scenes_current_scene_to_cstr() {
    SceneType scene_type = g_scenes.current_scene_type;

    if (g_scenes.current_overlay_scene_type != SCENE_NONE) { scene_type = g_scenes.current_overlay_scene_type; }

    return i_scene_type_strings[scene_type];
}

String *scenes_to_str(SceneType scene_type) {
    C8 const *cstr = i_scene_type_strings[scene_type];
    String *string = TS("%s", cstr);
    string_replace_all(string, "_", " ");
    string_to_capital(string);
    return string;
}
