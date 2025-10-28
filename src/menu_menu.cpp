#include "menu.hpp"
#include "assert.hpp"
#include "core.hpp"
#include "scene.hpp"

enum MenuChoice : U8 {
    MENU_CHOICE_OVERWORLD,
    MENU_CHOICE_DUNGEON,
    MENU_CHOICE_COLLISION_TEST,
    MENU_CHOICE_PARTICLES_DEMO,
    MENU_CHOICE_EASE_DEMO,
    MENU_CHOICE_FADE_DEMO,
    MENU_CHOICE_OPTIONS,
    MENU_CHOICE_EXIT,
    MENU_CHOICE_COUNT,
};

SZ menu_extra_menu_func_get_entry_count() {
    return MENU_CHOICE_COUNT;
}

C8 static const *i_labels[MENU_CHOICE_COUNT] = {
    "Overworld",
    "Dungeon",
    "Collision Test",
    "Particles Demo",
    "Ease Demo",
    "Fade Demo",
    "Options",
    "Exit",
};

C8 const *menu_extra_menu_func_get_label(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr) {  // NOLINT
    unused(cb_data);
    unused(color_ptr);
    unused(color_override_ptr);

    return i_labels[idx];
}

C8 const *menu_extra_menu_func_get_desc(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr) {  // NOLINT
    unused(cb_data);
    unused(color_ptr);
    unused(color_override_ptr);

    return "";
}

void menu_extra_menu_func_yes(void *cb_data) {
    auto selected_entry = *(SZ *)cb_data;

    switch (selected_entry) {
        case MENU_CHOICE_OVERWORLD: {
            scenes_set_scene(SCENE_OVERWORLD);
        } break;
        case MENU_CHOICE_DUNGEON: {
            scenes_set_scene(SCENE_DUNGEON);
        } break;
        case MENU_CHOICE_COLLISION_TEST: {
            scenes_set_scene(SCENE_COLLISION_TEST);
        } break;
        case MENU_CHOICE_PARTICLES_DEMO: {
            scenes_set_scene(SCENE_PARTICLES_DEMO);
        } break;
        case MENU_CHOICE_EASE_DEMO: {
            scenes_set_scene(SCENE_EASE_DEMO);
        } break;
        case MENU_CHOICE_FADE_DEMO: {
            scenes_set_scene(SCENE_FADE_DEMO);
        } break;
        case MENU_CHOICE_OPTIONS: {
            scenes_push_overlay_scene(SCENE_OPTIONS);
        } break;
        case MENU_CHOICE_EXIT: {
            g_core.running = false;
        } break;
        default: {
            _unreachable_();
        }
    }
}
