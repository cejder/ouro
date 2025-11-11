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
#include <raymath.h>

EditState static i_state = {};

// ============================================================================
// Multi-selection helper functions
// ============================================================================

void static world_clear_selection() {
    g_world->selected_entity_count = 0;
}

void static world_add_to_selection(EID id) {
    // Check if already selected
    for (SZ i = 0; i < g_world->selected_entity_count; ++i) {
        if (g_world->selected_entities[i] == id) {
            return; // Already selected
        }
    }

    // Add to selection
    if (g_world->selected_entity_count < WORLD_MAX_ENTITIES) {
        g_world->selected_entities[g_world->selected_entity_count++] = id;
    }
}

void static world_remove_from_selection(EID id) {
    for (SZ i = 0; i < g_world->selected_entity_count; ++i) {
        if (g_world->selected_entities[i] == id) {
            // Shift remaining elements
            for (SZ j = i; j < g_world->selected_entity_count - 1; ++j) {
                g_world->selected_entities[j] = g_world->selected_entities[j + 1];
            }
            g_world->selected_entity_count--;

            return;
        }
    }
}

BOOL static world_is_selected(EID id) {
    for (SZ i = 0; i < g_world->selected_entity_count; ++i) {
        if (g_world->selected_entities[i] == id) {
            return true;
        }
    }
    return false;
}

void static world_set_single_selection(EID id) {
    world_clear_selection();
    world_add_to_selection(id);
}

void static world_toggle_selection(EID id) {
    if (world_is_selected(id)) {
        world_remove_from_selection(id);
    } else {
        world_add_to_selection(id);
    }
}

// Check if entity position is inside screen-space rectangle
BOOL static is_entity_in_screen_rect(EID id, Vector2 rect_min, Vector2 rect_max, Camera camera) {
    Vector3 const world_pos = g_world->position[id];
    Vector2 const screen_pos = GetWorldToScreen(world_pos, camera);

    return (screen_pos.x >= rect_min.x && screen_pos.x <= rect_max.x &&
            screen_pos.y >= rect_min.y && screen_pos.y <= rect_max.y);
}

// Helper function to print entity command messages with entity name
void static i_print_entity_command_message(EID id, C8 const *command_text, C8 const *command_color, MessageType type) {
    // C8 const *entity_name = g_world->name[id];

    // // If entity has a name, use it; otherwise use a generic description
    // if (entity_name[0] != '\0') {
    //     messages_ouc_printf(type, WHITE, MESSAGE_DEFAULT_DURATION,
    //         "\\ouc{#ffffffff}%s: \\ouc{%s}%s", entity_name, command_color, command_text);
    // } else {
    //     messages_ouc_printf(type, WHITE, MESSAGE_DEFAULT_DURATION,
    //         "\\ouc{#ffffffff}Entity: \\ouc{%s}%s", command_color, command_text);
    // }
}

// Helper function to print entity command messages with target entity name
void static i_print_entity_command_message_with_target(EID id, C8 const *command_text, C8 const *command_color, EID target_id, MessageType type) {
    // C8 const *entity_name = g_world->name[id];
    // C8 const *target_name = g_world->name[target_id];

    // if (entity_name[0] != '\0' && target_name[0] != '\0') {
    //     messages_ouc_printf(type, WHITE, MESSAGE_DEFAULT_DURATION,
    //         "\\ouc{#ffffffff}%s: \\ouc{%s}%s \\ouc{#888888ff}> \\ouc{#ffdd00ff}%s",
    //         entity_name, command_color, command_text, target_name);
    // } else if (entity_name[0] != '\0') {
    //     messages_ouc_printf(type, WHITE, MESSAGE_DEFAULT_DURATION,
    //         "\\ouc{#ffffffff}%s: \\ouc{%s}%s \\ouc{#888888ff}> \\ouc{#aaaaaa88}Entity #%d",
    //         entity_name, command_color, command_text, target_id);
    // } else if (target_name[0] != '\0') {
    //     messages_ouc_printf(type, WHITE, MESSAGE_DEFAULT_DURATION,
    //         "\\ouc{#ffffffff}Entity: \\ouc{%s}%s \\ouc{#888888ff}> \\ouc{#ffdd00ff}%s",
    //         command_color, command_text, target_name);
    // } else {
    //     messages_ouc_printf(type, WHITE, MESSAGE_DEFAULT_DURATION,
    //         "\\ouc{#ffffffff}Entity: \\ouc{%s}%s \\ouc{#888888ff}> \\ouc{#aaaaaa88}Entity #%d",
    //         command_color, command_text, target_id);
    // }
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

    // Keyboard rotation - apply rotation delta to this entity
    F32 rotation_delta = 0.0F;
    if (is_down(IA_DBG_ROTATE_ENTITY_NEG)) { rotation_delta -= rot_speed; }
    if (is_down(IA_DBG_ROTATE_ENTITY_POS)) { rotation_delta += rot_speed; }
    if (rotation_delta != 0.0F) {
        entity_set_rotation(id, rotation + rotation_delta);
    }

    // Keyboard movement - apply same movement to this entity
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
            if (id == g_world->selected_entities[0]) {  // Only update click location for primary
                i_state.mouse_click_location = terrain_collision.point;
            }
        }
    }

    // Mouse interactions (only if collision is valid)
    // NOTE: For multi-entity operations, only the first entity's position/scale matters,
    // and we apply the same delta to all selected entities
    if (mouse_left_down && collision.hit) {
        // Mouse rotation with Alt - apply rotation delta to this entity
        if (is_mod(I_MODIFIER_ALT)) {
            Vector2 const mouse_delta = input_get_mouse_delta();
            entity_set_rotation(id, rotation + (mouse_delta.x * 0.5F));
            *left_click_consumed = true;
        }
        // Mouse dragging with Ctrl (but not Ctrl+Shift, which is scaling)
        else if (is_mod(I_MODIFIER_CTRL) && !is_mod(I_MODIFIER_SHIFT)) {
            entity_set_position(id, collision.point);
            collision.point.y   += 1.0F;
            *left_click_consumed = true;
        }
        // Mouse scaling with Ctrl+Shift - scale this entity by the same factor
        else if (is_mod(I_MODIFIER_SHIFT) && is_mod(I_MODIFIER_CTRL)) {
            Vector2 const mouse_delta = input_get_mouse_delta();
            F32 t = scale.x;

            // Right/Up increases scale, Left/Down decreases (invert Y since positive Y is down in Raylib)
            t += (mouse_delta.x - mouse_delta.y) * 0.01F;
            t  = glm::max(t, 0.1F);  // Clamp minimum scale

            entity_set_scale(id, {t, t, t});
            *left_click_consumed = true;
        }
    }

    // Mouse wheel for vertical movement (with Ctrl modifier) - apply to this entity
    if (is_mod(I_MODIFIER_CTRL)) {
        F32 const wheel = input_get_mouse_wheel();
        if (wheel != 0.0F) {
            F32 const mouse_scroll_modifier = 0.25F;
            entity_add_position(id, {0.0F, wheel * mouse_scroll_modifier, 0.0F});
        }
    }

    // Delete entity - handled separately for multi-selection to avoid array modification during iteration
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
        if (g_world->selected_entity_count == 0) {
            particles3d_add_click_indicator(collision.point, 0.5F, PINK, PURPLE, 5);
        }
    }

    // Rectangle selection (left mouse drag)
    BOOL const shift_down = is_mod(I_MODIFIER_SHIFT);

    // Entity manipulation (only if entity is selected)
    // Allow manipulation unless Shift-only is held (reserved for selection)
    // Ctrl+Shift is allowed for entity scaling
    if (g_world->selected_entity_count > 0 && (!shift_down || ctrl_down)) {
        // For multi-entity selection with Ctrl+drag, maintain relative positions
        BOOL const is_multi_selection = g_world->selected_entity_count > 1;
        Vector3 position_offset = {0.0F, 0.0F, 0.0F};

        // Calculate position offset from first entity if doing Ctrl+drag
        if (is_multi_selection && mouse_left_down && collision.hit &&
            is_mod(I_MODIFIER_CTRL) && !is_mod(I_MODIFIER_SHIFT) && !is_mod(I_MODIFIER_ALT)) {
            EID const first_id = g_world->selected_entities[0];
            Vector3 const old_pos = g_world->position[first_id];
            position_offset = Vector3Subtract(collision.point, old_pos);
        }

        // Apply manipulation to all selected entities
        for (SZ i = 0; i < g_world->selected_entity_count; ++i) {
            EID const id = g_world->selected_entities[i];

            // For Ctrl+drag with multiple entities, apply offset instead of direct position
            if (is_multi_selection && mouse_left_down && collision.hit &&
                is_mod(I_MODIFIER_CTRL) && !is_mod(I_MODIFIER_SHIFT) && !is_mod(I_MODIFIER_ALT)) {
                // Apply the same offset to maintain relative positions
                Vector3 new_pos = Vector3Add(g_world->position[id], position_offset);
                entity_set_position(id, new_pos);
                left_click_consumed = true;
            } else {
                // Regular handling for other operations
                i_handle_selected_entity_input(id, dtu, mouse_left_down, collision, &left_click_consumed);
            }
        }

        // Delete all selected entities (done after iteration to avoid modifying array during loop)
        if (is_pressed(IA_DBG_DELETE_ENTITY)) {
            // Copy selected entities to temporary array since entity_destroy modifies selection
            EID entities_to_delete[WORLD_MAX_ENTITIES];
            SZ delete_count = g_world->selected_entity_count;
            for (SZ i = 0; i < delete_count; ++i) {
                entities_to_delete[i] = g_world->selected_entities[i];
            }
            // Now delete all of them
            for (SZ i = 0; i < delete_count; ++i) {
                entity_destroy(entities_to_delete[i]);
            }
        }
    }

    if (!left_click_consumed && !mouse_look && mouse_left_pressed) {
        // Start potential rectangle selection
        i_state.is_selecting_rect = true;
        i_state.selection_rect_start = input_get_mouse_position_screen();
        i_state.selection_rect_end = i_state.selection_rect_start;
    }

    if (i_state.is_selecting_rect && mouse_left_down) {
        // Update rectangle end point
        i_state.selection_rect_end = input_get_mouse_position_screen();
    }

    if (i_state.is_selecting_rect && !mouse_left_down) {
        // Finish rectangle selection
        Vector2 const rect_start = i_state.selection_rect_start;
        Vector2 const rect_end = i_state.selection_rect_end;
        F32 const drag_distance = Vector2Distance(rect_start, rect_end);
        F32 const min_drag_threshold = 5.0F; // Minimum pixels to count as rectangle selection

        if (drag_distance > min_drag_threshold) {
            // Rectangle selection - select all entities in rectangle
            Vector2 rect_min = {glm::min(rect_start.x, rect_end.x), glm::min(rect_start.y, rect_end.y)};
            Vector2 rect_max = {glm::max(rect_start.x, rect_end.x), glm::max(rect_start.y, rect_end.y)};

            // Clear selection if not holding shift
            if (!shift_down) {
                world_clear_selection();
            }

            // Select all entities in rectangle
            Camera const current_camera = g_player.cameras[g_scenes.current_scene_type];
            for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
                EID const id = g_world->active_entities[idx];

                // Only select NPCs (unless debug mode)
                if (!c_debug__enabled && g_world->type[id] != ENTITY_TYPE_NPC) {
                    continue;
                }

                if (is_entity_in_screen_rect(id, rect_min, rect_max, current_camera)) {
                    world_add_to_selection(id);
                }
            }
        } else {
            // Single click selection
            EID const entity_id = c_debug__enabled ? entity_find_at_mouse() : entity_find_at_mouse_with_type(ENTITY_TYPE_NPC);

            if (entity_id != INVALID_EID) {
                if (shift_down) {
                    // Shift+click: toggle selection
                    world_toggle_selection(entity_id);
                } else {
                    // Normal click: replace selection
                    world_set_single_selection(entity_id);
                }
            } else if (!shift_down) {
                // Clicked empty space without shift: clear selection
                world_clear_selection();
            }
        }

        i_state.is_selecting_rect = false;
    }

    // Center screen selection (mouse look or hotkey)
    if ((mouse_look && mouse_left_pressed) || is_pressed(IA_DBG_SELECT_ENTITY)) {
        EID const entity_id = c_debug__enabled ? entity_find_at_screen_point(render_get_center_of_render())
                                         : entity_find_at_screen_point_with_type(render_get_center_of_render(), ENTITY_TYPE_NPC);

        if (entity_id != INVALID_EID) {
            if (shift_down) {
                world_toggle_selection(entity_id);
            } else {
                world_set_single_selection(entity_id);
            }
        }
    }

    // Clear selection with middle mouse
    if (mouse_middle_pressed) { world_clear_selection(); }

    // Right click commands on selected entities (apply to ALL selected)
    if (mouse_right_pressed && g_world->selected_entity_count > 0 && collision.hit) {
        // Spawn particles at the click location
        particles3d_add_click_indicator(collision.point, 0.5F, PINK, PURPLE, 5);

        if (ctrl_down) {
            // Gather command for all selected units
            for (SZ i = 0; i < g_world->selected_entity_count; ++i) {
                EID const id = g_world->selected_entities[i];
                entity_actor_start_looking_for_target(id, ENTITY_TYPE_VEGETATION);
            }
            i_print_entity_command_message(g_world->selected_entities[0], "Gathering resources", "#ffaa00ff", MESSAGE_TYPE_WARN);
        } else {
            // Check if we clicked on an entity
            EID const clicked_entity = entity_find_at_mouse();

            if (clicked_entity != INVALID_EID && !world_is_selected(clicked_entity)) {
                // We clicked on a different entity - attack it if it's an NPC
                if (g_world->type[clicked_entity] == ENTITY_TYPE_NPC) {
                    for (SZ i = 0; i < g_world->selected_entity_count; ++i) {
                        EID const id = g_world->selected_entities[i];
                        entity_actor_set_attack_target_npc(id, clicked_entity);
                    }
                    i_print_entity_command_message_with_target(g_world->selected_entities[0], "Attacking", "#ff3030ff", clicked_entity, MESSAGE_TYPE_ERROR);
                } else {
                    // Not an NPC, just move to the position
                    for (SZ i = 0; i < g_world->selected_entity_count; ++i) {
                        EID const id = g_world->selected_entities[i];
                        entity_actor_set_move_target(id, collision.point);
                    }
                    i_print_entity_command_message(g_world->selected_entities[0], "Moving to position", "#00ff88ff", MESSAGE_TYPE_INFO);
                }
            } else {
                // No entity clicked, or clicked on self - just move to position
                for (SZ i = 0; i < g_world->selected_entity_count; ++i) {
                    EID const id = g_world->selected_entities[i];
                    entity_actor_set_move_target(id, collision.point);
                }
                i_print_entity_command_message(g_world->selected_entities[0], "Moving to position", "#00ff88ff", MESSAGE_TYPE_INFO);
            }
        }
    }

    // Spawn selection indicator particles for all selected entities (frame-rate independent)
    if (g_world->selected_entity_count > 0) {
        // Frame-rate independent particle spawning
        F32 const spawn_interval = 1.0F / EDIT_SELECTION_INDICATOR_PARTICLES_PER_SECOND;
        i_state.selection_indicator_timer += dt;
        while (i_state.selection_indicator_timer >= spawn_interval) {
            for (SZ i = 0; i < g_world->selected_entity_count; ++i) {
                EID const id = g_world->selected_entities[i];
                Vector3 const position = g_world->position[id];
                F32 const radius = g_world->radius[id];
                particles3d_add_selection_indicator(position, radius, GREEN, LIME, EDIT_SELECTION_INDICATOR_PARTICLE_COUNT);
            }
            i_state.selection_indicator_timer -= spawn_interval;
        }
    } else {
        // Reset timer when no entity is selected
        i_state.selection_indicator_timer = 0.0F;
    }
}


void edit_draw_2d_hud() {
    // Draw selection rectangle while dragging
    if (i_state.is_selecting_rect) {
        Vector2 const start = i_state.selection_rect_start;
        Vector2 const end = i_state.selection_rect_end;

        // Calculate rectangle bounds
        F32 const x = glm::min(start.x, end.x);
        F32 const y = glm::min(start.y, end.y);
        F32 const width = glm::abs(end.x - start.x);
        F32 const height = glm::abs(end.y - start.y);

        // Draw filled rectangle with transparency
        Color const fill_color = {100, 200, 100, 50};  // Semi-transparent green
        DrawRectangle((S32)x, (S32)y, (S32)width, (S32)height, fill_color);

        // Draw border
        Color const border_color = {100, 255, 100, 200};  // Bright green border
        DrawRectangleLines((S32)x, (S32)y, (S32)width, (S32)height, border_color);
    }
}

Vector3 edit_get_last_click_world_position() {
    return i_state.mouse_click_location;
}
