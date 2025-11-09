#include "assert.hpp"
#include "cvar.hpp"
#include "entity_spawn.hpp"
#include "input.hpp"
#include "menu.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "scene_constants.hpp"
#include "string.hpp"
#include "world.hpp"

#include <raymath.h>

enum MenuChoice : U8 {
    MENU_CHOICE_NOCLIP,
    MENU_CHOICE_DEBUG,
    MENU_CHOICE_SKYBOX,
    MENU_CHOICE_SKYBOX_NIGHT,
    MENU_CHOICE_SKETCH_EFFECT,
    MENU_CHOICE_ADD_REMOVE_RANDOM_NPC,
    MENU_CHOICE_ADD_REMOVE_VEGETATION,
    MENU_CHOICE_RANDOMLY_ROTATE_ENTITIES,
    MENU_CHOICE_DUMP_ALL_ENTITIES,
    MENU_CHOICE_RESET_WORLD,
    MENU_CHOICE_SHOW_KEYBINDINGS,
    MENU_CHOICE_COUNT,
};

SZ menu_extra_overworld_func_get_entry_count() {
    return MENU_CHOICE_COUNT;
}

 C8 static const *i_labels[MENU_CHOICE_COUNT] = {
    "Noclip",
    "Debug",
    "Skybox",
    "Day Skybox",
    "Sketch Effect",
    "+/- NPC",
    "+/- Vegetation",
    "Rotate Entities",
    "Dump Entities",
    "Reset World",
    "Show Keybindings",
};

C8 const *menu_extra_overworld_func_get_label(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr) {  // NOLINT
    unused(cb_data);
    unused(color_ptr);
    unused(color_override_ptr);

    return i_labels[idx];
}

C8 const *menu_extra_overworld_func_get_desc(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr) {  // NOLINT
    unused(cb_data);
    unused(color_ptr);
    unused(color_override_ptr);

    return "";
}

C8 const *menu_extra_overworld_func_get_value(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr) {
    unused(cb_data);

    // NOTE: With white spaces for easier formatting. This is just development code.
    C8 const *on_text  = "ON";
    C8 const *off_text = "OFF";

    switch (idx) {
        case MENU_CHOICE_NOCLIP: {
            menu_extra_override_toggle_color(color_ptr, color_override_ptr, c_debug__noclip);
            return c_debug__noclip ? on_text : off_text;
        }
        case MENU_CHOICE_DEBUG: {
            menu_extra_override_toggle_color(color_ptr, color_override_ptr, c_debug__enabled);
            return c_debug__enabled ? on_text : off_text;
        }
        case MENU_CHOICE_SKYBOX: {
            menu_extra_override_toggle_color(color_ptr, color_override_ptr, c_render__skybox);
            return c_render__skybox ? on_text : off_text;
        }
        case MENU_CHOICE_SKYBOX_NIGHT: {
            menu_extra_override_toggle_color(color_ptr, color_override_ptr, c_render__skybox_night);
            return c_render__skybox_night ? on_text : off_text;
        }
        case MENU_CHOICE_SKETCH_EFFECT: {
            menu_extra_override_toggle_color(color_ptr, color_override_ptr, c_render__sketch_effect);
            return c_render__sketch_effect ? on_text : off_text;
        }
        case MENU_CHOICE_RANDOMLY_ROTATE_ENTITIES:
        case MENU_CHOICE_DUMP_ALL_ENTITIES: {
            return nullptr;
        }
        case MENU_CHOICE_ADD_REMOVE_RANDOM_NPC: {
            return TS("%u NPCs", g_world->entity_type_counts[ENTITY_TYPE_NPC])->c;
        }
        case MENU_CHOICE_ADD_REMOVE_VEGETATION: {
            return TS("%u vegetation", g_world->entity_type_counts[ENTITY_TYPE_VEGETATION])->c;
        }
        case MENU_CHOICE_RESET_WORLD: {
            return nullptr;
        }
        case MENU_CHOICE_SHOW_KEYBINDINGS: {
            menu_extra_override_toggle_color(color_ptr, color_override_ptr, c_debug__keybindings_info);
            return c_debug__keybindings_info ? on_text : off_text;
        }
        default: {
            _unreachable_();
            return nullptr;
        }
    }
}

void menu_extra_overworld_func_yes(void *_cb_data) {
    auto selected_entry = *(SZ *)_cb_data;

    SZ const count        = 1000;

    switch (selected_entry) {
        case MENU_CHOICE_NOCLIP: {
            MenuExtraToggleData toggle_data = {&c_debug__noclip, "Noclip"};
            menu_extra_toggle_data_cb_toggle(&toggle_data);
        } break;
        case MENU_CHOICE_DEBUG: {
            MenuExtraToggleData toggle_data = {&c_debug__enabled, "Debug"};
            menu_extra_toggle_data_cb_toggle(&toggle_data);
        } break;
        case MENU_CHOICE_SKYBOX: {
            MenuExtraToggleData toggle_data = {&c_render__skybox, "Skybox"};
            menu_extra_toggle_data_cb_toggle(&toggle_data);
        } break;
        case MENU_CHOICE_SKYBOX_NIGHT: {
            MenuExtraToggleData toggle_data = {&c_render__skybox_night, "Night Skybox"};
            menu_extra_toggle_data_cb_toggle(&toggle_data);
        } break;
        case MENU_CHOICE_SKETCH_EFFECT: {
            MenuExtraToggleData toggle_data = {&c_render__sketch_effect, "Sketch Effect"};
            menu_extra_toggle_data_cb_toggle(&toggle_data);
        } break;
        case MENU_CHOICE_ADD_REMOVE_RANDOM_NPC: {
            entity_spawn_random_npc_around_camera(count, false);
        } break;
        case MENU_CHOICE_ADD_REMOVE_VEGETATION: {
            entity_spawn_random_vegetation_on_terrain(g_world->base_terrain, count, false);
        } break;
        case MENU_CHOICE_RANDOMLY_ROTATE_ENTITIES: {
            world_randomly_rotate_entities();
        } break;
        case MENU_CHOICE_DUMP_ALL_ENTITIES: {
            world_dump_all_entities();
            c_console__enabled = true;
        } break;
        case MENU_CHOICE_RESET_WORLD: {
            world_reset();
        } break;
        case MENU_CHOICE_SHOW_KEYBINDINGS: {
            MenuExtraToggleData toggle_data = {&c_debug__keybindings_info, "Show Keybindings"};
            menu_extra_toggle_data_cb_toggle(&toggle_data);
        } break;

        default: {
            _unreachable_();
        }
    }
}

void menu_extra_overworld_func_change(void *_cb_data, BOOL increase) {
    auto selected_entry = *(SZ *)_cb_data;

    BOOL const shift_down = is_mod(I_MODIFIER_SHIFT);
    SZ const count = shift_down ? 100 : 1;
    BOOL const report = !shift_down;

    switch (selected_entry) {
        case MENU_CHOICE_ADD_REMOVE_RANDOM_NPC: {
            if (increase) {
                entity_spawn_random_npc_around_camera(count, report);
            } else {
                entity_despawn_random_npc_around_camera(count, report);
            }
        } break;
        case MENU_CHOICE_ADD_REMOVE_VEGETATION: {
            if (increase) {
                entity_spawn_random_vegetation_on_terrain(g_world->base_terrain, count, report);
            } else {
                entity_despawn_random_vegetation(count, report);
            }
        } break;
        default: {
            // For the rest, We are just going to use the yes function for this.
            menu_extra_overworld_func_yes(_cb_data);
        } break;
    }
}
