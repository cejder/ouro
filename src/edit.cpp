#include "edit.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "audio.hpp"
#include "color.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "input.hpp"
#include "message.hpp"
#include "particles_3d.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "world.hpp"

#include <glm/gtc/type_ptr.hpp>

EditState static i_state = {};

// Helper function to print entity command messages with entity name
void static i_print_entity_command_message(EID id, C8 const *command_text, C8 const *command_color, MessageType type) {
    C8 const *entity_name = g_world->name[id];

    // If entity has a name, use it; otherwise use a generic description
    if (entity_name[0] != '\0') {
        messages_ouc_printf(type, WHITE, MESSAGE_DEFAULT_DURATION,
            "\\ouc{#ffffffff}%s: \\ouc{%s}%s", entity_name, command_color, command_text);
    } else {
        messages_ouc_printf(type, WHITE, MESSAGE_DEFAULT_DURATION,
            "\\ouc{#ffffffff}Entity: \\ouc{%s}%s", command_color, command_text);
    }
}

// Helper function to print entity command messages with target entity name
void static i_print_entity_command_message_with_target(EID id, C8 const *command_text, C8 const *command_color, EID target_id, MessageType type) {
    C8 const *entity_name = g_world->name[id];
    C8 const *target_name = g_world->name[target_id];

    if (entity_name[0] != '\0' && target_name[0] != '\0') {
        messages_ouc_printf(type, WHITE, MESSAGE_DEFAULT_DURATION,
            "\\ouc{#ffffffff}%s: \\ouc{%s}%s \\ouc{#888888ff}> \\ouc{#ffdd00ff}%s",
            entity_name, command_color, command_text, target_name);
    } else if (entity_name[0] != '\0') {
        messages_ouc_printf(type, WHITE, MESSAGE_DEFAULT_DURATION,
            "\\ouc{#ffffffff}%s: \\ouc{%s}%s \\ouc{#888888ff}> \\ouc{#aaaaaa88}Entity #%d",
            entity_name, command_color, command_text, target_id);
    } else if (target_name[0] != '\0') {
        messages_ouc_printf(type, WHITE, MESSAGE_DEFAULT_DURATION,
            "\\ouc{#ffffffff}Entity: \\ouc{%s}%s \\ouc{#888888ff}> \\ouc{#ffdd00ff}%s",
            command_color, command_text, target_name);
    } else {
        messages_ouc_printf(type, WHITE, MESSAGE_DEFAULT_DURATION,
            "\\ouc{#ffffffff}Entity: \\ouc{%s}%s \\ouc{#888888ff}> \\ouc{#aaaaaa88}Entity #%d",
            command_color, command_text, target_id);
    }
}

void static i_handle_selected_entity_input(EID id, F32 dtu, BOOL mouse_left_down, RayCollision collision, BOOL *left_click_consumed) {
    F32 const base_move_speed = 5.0F;
    F32 const rotation_speed  = 30.0F;
    F32 move_multiplier       = 1.0F;

    // Calculate speed multipliers
    if (is_mod(I_MODIFIER_SHIFT)) {
        move_multiplier = 5.0F;
    } else if (is_mod(I_MODIFIER_CTRL)) {
        move_multiplier = 0.1F;
    }

    F32 const move_speed = base_move_speed * move_multiplier * dtu;
    F32 const rot_speed  = rotation_speed * move_multiplier * dtu;
    F32 const rotation   = g_world->rotation[id];
    Vector3 position     = g_world->position[id];
    Vector3 const scale  = g_world->scale[id];

    // Keyboard rotation
    if (is_down(IA_DBG_ROTATE_ENTITY_NEG)) { entity_set_rotation(id, rotation - rot_speed); }
    if (is_down(IA_DBG_ROTATE_ENTITY_POS)) { entity_set_rotation(id, rotation + rot_speed); }

    // Keyboard movement
    if (is_down(IA_DBG_MOVE_ENTITY_UP))       { entity_add_position(id, {0,  move_speed, 0}); }
    if (is_down(IA_DBG_MOVE_ENTITY_DOWN))     { entity_add_position(id, {0, -move_speed, 0}); }
    if (is_down(IA_DBG_MOVE_ENTITY_LEFT))     { entity_move(id, MOVE_LEFT,     move_speed);   }
    if (is_down(IA_DBG_MOVE_ENTITY_RIGHT))    { entity_move(id, MOVE_RIGHT,    move_speed);   }
    if (is_down(IA_DBG_MOVE_ENTITY_FORWARD))  { entity_move(id, MOVE_FORWARD,  move_speed);   }
    if (is_down(IA_DBG_MOVE_ENTITY_BACKWARD)) { entity_move(id, MOVE_BACKWARD, move_speed);   }

    // Snap to ground
    if (is_pressed(IA_DBG_MOVE_ENTITY_TO_GROUND)) {
        RayCollision const terrain_collision = math_ray_collision_to_terrain(g_world->base_terrain, position, {0.0F, -1.0F, 0.0F});
        if (terrain_collision.hit) {
            position.y = terrain_collision.point.y;
            entity_set_position(id, position);
            i_state.mouse_click_location = terrain_collision.point;
        }
    }

    // Mouse interactions (only if collision is valid)
    if (mouse_left_down && collision.hit) {
        // Mouse rotation with Alt
        if (is_mod(I_MODIFIER_ALT)) {
            Vector2 const mouse_delta = input_get_mouse_delta();
            entity_set_rotation(id, rotation + (mouse_delta.x * 0.5F));
            *left_click_consumed = true;
        }
        // Mouse dragging with Ctrl
        else if (is_mod(I_MODIFIER_CTRL)) {
            entity_set_position(id, collision.point);
            collision.point.y   += 1.0F;
            *left_click_consumed = true;
        }
        // Mouse scaling with Shift
        else if (is_mod(I_MODIFIER_SHIFT)) {
            Vector2 const mouse_delta = input_get_mouse_delta();
            F32 t = scale.x;

            // Right/Up increases scale, Left/Down decreases (invert Y since positive Y is down in Raylib)
            t += (mouse_delta.x - mouse_delta.y) * 0.01F;
            t  = glm::max(t, 0.1F);  // Clamp minimum scale

            entity_set_scale(id, {t, t, t});
            *left_click_consumed = true;
        }
    }

    // Mouse wheel for vertical movement (with Ctrl modifier)
    if (is_mod(I_MODIFIER_CTRL)) {
        F32 const wheel = input_get_mouse_wheel();
        if (wheel != 0.0F) {
            F32 const mouse_scroll_modifier = 0.25F;
            entity_add_position(id, {0.0F, wheel * mouse_scroll_modifier, 0.0F});
        }
    }

    // Delete entity
    if (is_pressed(IA_DBG_DELETE_ENTITY)) {
        // HACK: REMOVE THIS
        entity_destroy(id);
    }
}

void edit_update(F32 dt, F32 dtu) {
    // Mouse state tracking
    BOOL left_click_consumed  = false;
    BOOL const dbg_mouse_over = dbg_is_mouse_over_or_dragged();
    BOOL const ctrl_down      = is_mod(I_MODIFIER_CTRL);
    BOOL const mouse_look     = input_is_mouse_look_active();

    BOOL const mouse_left_pressed   = is_mouse_pressed(I_MOUSE_LEFT)   && !dbg_mouse_over;
    BOOL const mouse_left_down      = is_mouse_down(I_MOUSE_LEFT)      && !dbg_mouse_over;
    BOOL const mouse_right_pressed  = is_mouse_pressed(I_MOUSE_RIGHT)  && !dbg_mouse_over;
    BOOL const mouse_middle_pressed = is_mouse_pressed(I_MOUSE_MIDDLE) && !dbg_mouse_over;
    BOOL const mouse_side_down      = is_mouse_down(I_MOUSE_SIDE)      && !dbg_mouse_over;

    RayCollision collision = {};

    // Calculate ray collision for mouse interactions
    if (mouse_left_down || mouse_right_pressed) {
        Vector2 const mouse_pos = mouse_side_down ? render_get_center_of_window() : input_get_mouse_position_screen();
        Ray const ray           = GetScreenToWorldRay(mouse_pos, g_player.cameras[g_scenes.current_scene_type]);
        collision               = GetRayCollisionMesh(ray, g_world->base_terrain->mesh, g_world->base_terrain->transform);
    }

    // Right click to store the click location
    if (mouse_right_pressed && collision.hit) {
        i_state.mouse_click_location = collision.point;
        audio_play(ACG_SFX, "ting.ogg");

        // Spawn indicator particles at click location (only if no entity is selected, otherwise handled in command section)
        if (g_world->selected_id == INVALID_EID) {
            particles3d_add_effect_click_indicator(collision.point, 0.5F, PINK, PURPLE, 5);
        }
    }

    // Entity manipulation (only if entity is selected)
    if (g_world->selected_id != INVALID_EID) { i_handle_selected_entity_input(g_world->selected_id, dtu, mouse_left_down, collision, &left_click_consumed); }

    // Entity selection (only if click wasn't consumed)
    if (!left_click_consumed && !mouse_look && mouse_left_pressed) {
        EID const entity_id = c_debug__enabled ? entity_find_at_mouse() : entity_find_at_mouse_with_type(ENTITY_TYPE_NPC);

        if (entity_id != INVALID_EID) { world_set_selected_entity(entity_id); }
    }

    // Center screen selection (mouse look or hotkey)
    if ((mouse_look && mouse_left_pressed) || is_pressed(IA_DBG_SELECT_ENTITY)) {
        EID const entity_id = c_debug__enabled ? entity_find_at_screen_point(render_get_center_of_render())
                                         : entity_find_at_screen_point_with_type(render_get_center_of_render(), ENTITY_TYPE_NPC);

        if (entity_id != INVALID_EID) { world_set_selected_entity(entity_id); }
    }

    // Clear selection with middle mouse
    if (mouse_middle_pressed) { g_world->selected_id = INVALID_EID; }

    // Right click commands on selected entity
    if (mouse_right_pressed && g_world->selected_id != INVALID_EID && collision.hit) {
        // Spawn particles at the click location
        particles3d_add_effect_click_indicator(collision.point, 0.5F, PINK, PURPLE, 5);

        if (ctrl_down) {
            entity_actor_start_looking_for_target(g_world->selected_id, ENTITY_TYPE_VEGETATION);
            i_print_entity_command_message(g_world->selected_id, "Gathering resources", "#ffaa00ff", MESSAGE_TYPE_WARN);
        } else {
            // Check if we clicked on an entity
            EID const clicked_entity = entity_find_at_mouse();

            if (clicked_entity != INVALID_EID && clicked_entity != g_world->selected_id) {
                // We clicked on a different entity - attack it if it's an NPC
                if (g_world->type[clicked_entity] == ENTITY_TYPE_NPC) {
                    entity_actor_set_attack_target_npc(g_world->selected_id, clicked_entity);
                    i_print_entity_command_message_with_target(g_world->selected_id, "Attacking", "#ff3030ff", clicked_entity, MESSAGE_TYPE_ERROR);
                } else {
                    // Not an NPC, just move to the position
                    entity_actor_set_move_target(g_world->selected_id, collision.point);
                    i_print_entity_command_message(g_world->selected_id, "Moving to position", "#00ff88ff", MESSAGE_TYPE_INFO);
                }
            } else {
                // No entity clicked, or clicked on self - just move to position
                entity_actor_set_move_target(g_world->selected_id, collision.point);
                i_print_entity_command_message(g_world->selected_id, "Moving to position", "#00ff88ff", MESSAGE_TYPE_INFO);
            }
        }
    }

    // Spawn selection indicator particles for the selected entity (frame-rate independent)
    if (g_world->selected_id != INVALID_EID) {
        Vector3 const position  = g_world->position[g_world->selected_id];
        F32 const radius        = g_world->radius[g_world->selected_id];

        // Frame-rate independent particle spawning
        F32 const spawn_interval = 1.0F / EDIT_SELECTION_INDICATOR_PARTICLES_PER_SECOND;
        i_state.selection_indicator_timer += dt;
        while (i_state.selection_indicator_timer >= spawn_interval) {
            particles3d_add_effect_selection_indicator(position, radius, GREEN, LIME, EDIT_SELECTION_INDICATOR_PARTICLE_COUNT);
            i_state.selection_indicator_timer -= spawn_interval;
        }
    } else {
        // Reset timer when no entity is selected
        i_state.selection_indicator_timer = 0.0F;
    }
}


Vector3 edit_get_last_click_world_position() {
    return i_state.mouse_click_location;
}
