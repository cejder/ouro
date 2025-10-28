#pragma once

#include "common.hpp"

#include <raylib.h>

fwd_decl(World);
fwd_decl(Scene);
fwd_decl(Loading);
fwd_decl(String);

#define SCENE_DEFAULT_CLEAR_COLOR BLACK;

#define LIST_OF_SCENES                    \
    SCENE(INTRO, intro)                   \
    SCENE(MENU, menu)                     \
    SCENE(OPTIONS, options)               \
    SCENE(OVERLAY_MENU, overlay_menu)     \
    SCENE(OVERWORLD, overworld)           \
    SCENE(DUNGEON, dungeon)               \
    SCENE(PARTICLES_DEMO, particles_demo) \
    SCENE(COLLISION_TEST, collision_test) \
    SCENE(EASE_DEMO, ease_demo)           \
    SCENE(FADE_DEMO, fade_demo)

#define SCENE(name, _) SCENE_##name,
enum SceneType : U8 { SCENE_NONE, LIST_OF_SCENES SCENE_COUNT };
#undef SCENE

#define NUMBER_OF_SCENES SCENE_COUNT

#define SCENE_INIT(name)   void scene_##name##_init(Scene *scene)
#define SCENE_ENTER(name)  void scene_##name##_enter()
#define SCENE_RESUME(name) void scene_##name##_resume()
#define SCENE_EXIT(name)   void scene_##name##_exit()
#define SCENE_UPDATE(name) void scene_##name##_update(F32 dt, F32 dtu)
#define SCENE_DRAW(name)   void scene_##name##_draw()

#define SCENE(_, name)                           \
    void scene_##name##_init(Scene *scene);      \
    void scene_##name##_enter();                 \
    void scene_##name##_resume();                \
    void scene_##name##_exit();                  \
    void scene_##name##_update(F32 dt, F32 dtu); \
    void scene_##name##_draw();
LIST_OF_SCENES // NOLINT
#undef SCENE

using SceneInit   = void (*)(Scene *scene);
using SceneEnter  = void (*)();
using SceneResume = void (*)();
using SceneExit   = void (*)();
using SceneUpdate = void (*)(F32 dt, F32 dtu);
using SceneDraw   = void (*)();

struct Scene {
    SceneType type;
    SceneInit init;
    SceneEnter enter;
    SceneResume resume;
    SceneExit exit;
    SceneUpdate update;
    SceneDraw draw;
    Color clear_color;
};

struct Scenes {
    BOOL initialized;
    BOOL skip_draw;
    SceneType current_scene_type;
    SceneType current_overlay_scene_type;
    SceneType next_scene_type;
    SceneType next_overlay_scene_type;
    Scene scenes[SCENE_COUNT];
    BOOL initialized_scene[SCENE_COUNT];
};

Scenes extern g_scenes;

void scenes_init();
void scenes_set_scene(SceneType scene_type);
SceneType scenes_set_scene_by_name(C8 *scene_name);
void scenes_change_overlay_scene(SceneType scene_type);
void scenes_push_overlay_scene(SceneType scene_type);
void scenes_pop_overlay_scene(BOOL resume, BOOL skip_next_draw);
void scenes_update(F32 dt, F32 dtu);
void scenes_draw();
Color scenes_get_clear_color();
C8 const *scenes_to_cstr(SceneType scene_type);
C8 const *scenes_current_scene_to_cstr();
String *scenes_to_str(SceneType scene_type);
