#include "menu.hpp"
#include "assert.hpp"
#include "core.hpp"
#include "scene.hpp"

enum OverlayChoice : U8 {
    OVERLAY_CHOICE_RESUME,
    OVERLAY_CHOICE_OPTIONS,
    OVERLAY_CHOICE_QUIT_TO_MENU,
    OVERLAY_CHOICE_QUIT_TO_DESKTOP,
    OVERLAY_CHOICE_COUNT,
};

SZ menu_extra_overlay_func_get_entry_count() {
    return OVERLAY_CHOICE_COUNT;
}

C8 static const *i_labels[OVERLAY_CHOICE_COUNT] = {
    "Resume",
    "Options",
    "Quit to Menu",
    "Quit to Desktop",
};

C8 const *menu_extra_overlay_func_get_label(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr) {  // NOLINT
    unused(cb_data);
    unused(color_ptr);
    unused(color_override_ptr);

    return i_labels[idx];
}

C8 const *menu_extra_overlay_func_get_desc(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr) {  // NOLINT
    unused(cb_data);
    unused(color_ptr);
    unused(color_override_ptr);

    return "";
}

void menu_extra_overlay_func_yes(void *_cb_data) {
    auto selected_entry = *(SZ *)_cb_data;

    switch (selected_entry) {
        case OVERLAY_CHOICE_RESUME: {
            scenes_pop_overlay_scene(true, true);
        } break;
        case OVERLAY_CHOICE_OPTIONS: {
            scenes_push_overlay_scene(SCENE_OPTIONS);
        } break;
        case OVERLAY_CHOICE_QUIT_TO_MENU: {
            scenes_pop_overlay_scene(false, true);
            scenes_set_scene(SCENE_MENU);
        } break;
        case OVERLAY_CHOICE_QUIT_TO_DESKTOP: {
            g_core.running = false;
        } break;
        default: {
            _unreachable_();
        }
    }
}
