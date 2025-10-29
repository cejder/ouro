#include "talk.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "audio.hpp"
#include "color.hpp"
#include "console.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "hud.hpp"
#include "input.hpp"
#include "math.hpp"
#include "message.hpp"
#include "profiler.hpp"
#include "render.hpp"
#include "std.hpp"
#include "string.hpp"
#include "time.hpp"
#include "entity.hpp"

#include <raymath.h>
#include <unistd.h>
#include <glm/gtc/type_ptr.hpp>

#define MARGIN_BETWEEN_RESPONSES 5.0F
#define MARGIN_BETWEEN_SENTENCE_AND_RESPONSES 50.0F
#define MARGIN_BETWEEN_BORDER_AND_NAME_PERC 2.5
#define TALKER_MIN_DISTANCE 4.0F
#define TALKER_BREAK_DISTANCE (1.5F * TALKER_MIN_DISTANCE)
#define TALKER_CHAR_DELAY 0.015F
#define TALKER_TING_EVERY_N_CHAR 4

DialogueNode *find_dialogue_node_by_id(DialogueRoot *root, SZ nodes_count, C8 const *id, BOOL can_fail) {
    for (SZ i = 0; i < nodes_count; ++i) {
        if (string_equals_cstr(root[i].node_id, id)) { return &root[i]; }
    }

    if (!can_fail) { _assertf_(false, "We could not find node '%s' but should have", id); }

    return nullptr;
}

DialogueResponse *find_dialogue_response_by_id(DialogueNode *question, SZ response_count, C8 const *id, BOOL can_fail) {
    // If no responses or invalid node, return nullptr
    if (!question || response_count == 0) { return nullptr; }

    // Check each response up to response_count
    for (SZ i = 0; i < response_count; ++i) {
        if (question->responses[i].response_id && string_equals_cstr(question->responses[i].response_id, id)) { return &question->responses[i]; }
    }

    if (!can_fail) { _assertf_(false, "We could not find respones '%s' but should have", id); }

    return nullptr;
}

void talker_update(EntityTalker *talker, F32 dt, Vector3 position, F32 entity_width) {
    if (!talker->is_enabled) { return; }
    talker->cursor_timer += dt;

    Camera3D *camera         = c3d_get_ptr();
    F32 const distance       = Vector3Distance(camera->position, position);
    F32 const reach_distance = entity_width + TALKER_MIN_DISTANCE;
    talker->in_reach = distance <= reach_distance;

    Conversation *con  = &talker->conversation;
    DialogueNode *node = con->current_node_ptr;
    _assert_(node != nullptr, "While updating the talker _current_node_ptr was nullptr");

    // Break distance check.
    F32 const break_distance = entity_width + (TALKER_MIN_DISTANCE * 2);
    BOOL const should_break  = talker->is_active && (distance >= break_distance || is_pressed(IA_NO));  // It's harder to break than to start
    if (should_break) {
        talker->is_active        = false;
        con->current_node_ptr    = find_dialogue_node_by_id(con->root, con->nodes_count, T_START_CSTR, false);
        node->response_selection = 0;
    }

    // Check if we should reset the character count and time since last character.
    BOOL const should_reset = !talker->is_active;
    if (should_reset) {
        con->char_count_to_print  = 0;
        con->time_since_last_char = 0.0F;
    }

    // Cancel all of this if debug console is up.
    if (c_console__enabled) { return; }

    BOOL should_play_movement_sound  = false;
    BOOL should_play_selection_sound = false;

    BOOL const should_start = !talker->is_active && talker->in_reach && is_pressed(IA_YES);
    if (should_start) {
        talker->is_active = true;
        should_play_selection_sound = true;
    }

    if (!should_start && !talker->in_reach) { return; }

    BOOL const has_no_responses = node->response_count == 0;
    BOOL const has_responses    = !has_no_responses;

    BOOL const text_is_fully_printed = con->char_count_to_print == node->node_text->length;
    BOOL const should_continue       = talker->is_active && has_no_responses && is_pressed(IA_YES) && text_is_fully_printed;
    if (should_continue) {
        if (string_equals_cstr(node->node_id_next, T_QUIT_CSTR)) {
            talker->is_active        = false;
            con->current_node_ptr    = find_dialogue_node_by_id(con->root, con->nodes_count, T_START_CSTR, false);
            node->response_selection = 0;
        } else {
            con->current_node_ptr = find_dialogue_node_by_id(con->root, con->nodes_count, node->node_id_next->c, false);
        }

        should_play_selection_sound = true;

        // Reset the character count and time since last character.
        con->char_count_to_print  = 0;
        con->time_since_last_char = 0.0F;
    }

    BOOL const next_selection = talker->is_active && has_responses && is_pressed(IA_NEXT);
    if (next_selection) {
        node->response_selection++;
        node->response_selection  %= node->response_count;
        should_play_movement_sound = true;
    }

    BOOL const previous_selection = talker->is_active && has_responses && is_pressed(IA_PREVIOUS);
    if (previous_selection) {
        if (node->response_selection == 0) {
            node->response_selection = node->response_count - 1;
        } else {
            node->response_selection--;
        }

        should_play_movement_sound = true;
    }

    BOOL const made_selection = talker->is_active && has_responses && is_pressed(IA_YES) && text_is_fully_printed;
    if (made_selection) {
        DialogueResponse *response = &node->responses[node->response_selection];

        if (response->trigger_func) { response->trigger_func(response->trigger_data); }

        if (string_equals_cstr(node->responses[node->response_selection].node_id_next, T_QUIT_CSTR)) {
            talker->is_active        = false;
            con->current_node_ptr    = find_dialogue_node_by_id(con->root, con->nodes_count, T_START_CSTR, false);
            node->response_selection = 0;
        } else {
            con->current_node_ptr = find_dialogue_node_by_id(con->root, con->nodes_count, node->responses[node->response_selection].node_id_next->c, false);
        }

        should_play_selection_sound = true;

        // Reset the character count and time since last character.
        con->char_count_to_print  = 0;
        con->time_since_last_char = 0.0F;
    }

    if (should_play_selection_sound) {
        audio_reset(ACG_SFX);
        audio_play(ACG_SFX, "menu_selection.ogg");
    }

    if (should_play_movement_sound) {
        audio_reset(ACG_SFX);
        audio_play(ACG_SFX, "menu_movement.ogg");
    }
}

void talker_draw(EntityTalker *talker, C8 const *name) {
    if (!talker->is_enabled) { return; }

    Vector2 const res = render_get_render_resolution();

    // Draw icon if in reach and not active yet.
    if (talker->in_reach && !talker->is_active) {
        ATexture *talker_icon = asset_get_texture("cursor_help.png");
        Vector2 const icon_position = {
            (res.x / 2.0F) - ((F32)talker_icon->base.width / 2.0F),
            (res.y / 2.0F) - ((F32)talker_icon->base.height / 2.0F) - 50.0F,
        };

        d2d_texture_v(talker_icon, icon_position, WHITE);
    }

    if (talker->is_active) {
        auto major_color                 = g_render.sketch.major_color;
        auto minor_color                 = g_render.sketch.minor_color;
        auto box_color                   = Fade(minor_color, 0.85F);
        auto shadow_color                = BLACK;
        auto name_color                  = major_color;
        auto continue_color              = color_sync_blinking_fast(ORANGE, RED);
        auto sentence_color              = WHITE;
        auto cursor_color                = color_sync_blinking_fast(ORANGE, GOLD);
        auto response_color              = CREAM;
        auto selected_response_color     = color_sync_blinking_fast(GOLD, ORANGE);
        Vector2 const text_shadow_offset = ui_shadow_offset(0.075F, 0.105F);
        AFont *node_font                 = asset_get_font("GoMono", ui_font_size(2.4F));
        AFont *response_font             = asset_get_font("GoMono", ui_font_size(2.4F));
        AFont *response_icon_font        = asset_get_font("GoMono", ui_font_size(2.0));
        AFont *name_font                 = asset_get_font("GoMono", ui_font_size(2.5));
        Vector2 const padding            = {0.30F, 0.20F};

        // Calculate size and position of the dialogue box
        F32 const perc_x = (1.0F - (g_hud.left_width_perc + g_hud.right_width_perc + padding.x)) * 100.0F;
        F32 const perc_y = (1.0F - (g_hud.top_height_perc + g_hud.bottom_height_perc + padding.y)) * 100.0F;
        Vector2 const box_size = {
            ui_scale_x(perc_x),
            ui_scale_y(perc_y),
        };
        Vector2 const box_position = {
            ui_scale_x(g_hud.left_width_perc * 100.0F) + ui_scale_x(padding.x / 2 * 100.0F),
            ui_scale_y(g_hud.top_height_perc * 100.0F) + ui_scale_y(padding.y / 2 * 100.0F),
        };

        Conversation *con  = &talker->conversation;
        DialogueNode *node = find_dialogue_node_by_id(con->root, con->nodes_count, con->current_node_ptr->node_id->c, false);

        // Draw the background rectangle
        F32 const border_width     = 1.0F;
        F32 const roundness        = 0.033F;
        S32 const segments         = 16;
        Rectangle const border_rec = {
            box_position.x - border_width, box_position.y - border_width,
            box_size.x + (border_width * 2.0F),
            box_size.y + (border_width * 2.0F),
        };
        d2d_rectangle_rounded_rec(border_rec, roundness, segments, box_color);

        // Sentence
        if (con->char_count_to_print < node->node_text->length) {
            con->time_since_last_char += time_get_delta();

            if (con->time_since_last_char >= TALKER_CHAR_DELAY) {
                con->time_since_last_char = 0.0F;
                con->char_count_to_print++;
                // Play a sound for every Nth characters printed.
                if (con->char_count_to_print % TALKER_TING_EVERY_N_CHAR == 0) {
                    audio_reset(ACG_SFX);
                    audio_play(ACG_SFX, "ting.ogg");
                }
            }
        }

        C8 const *sentence      = node->node_text->c;
        Vector2 text_dimensions = measure_text(node_font, sentence);
        // NOTE: We still use the full sentence for the text dimensions.
        if (con->char_count_to_print < node->node_text->length) { sentence = ou_strn(sentence, con->char_count_to_print, MEMORY_TYPE_ARENA_TRANSIENT); }

        Vector2 const text_position = {
            box_position.x + (box_size.x / 2.0F) - (text_dimensions.x / 2.0F),
            box_position.y + (box_size.y / 2.0F) - (text_dimensions.y / 2.0F),
        };
        d2d_text_shadow(node_font, sentence, text_position, sentence_color, shadow_color, text_shadow_offset);

        // Name
        d2d_text_shadow(name_font, name,
                        {box_position.x + ui_scale_x(MARGIN_BETWEEN_BORDER_AND_NAME_PERC), box_position.y + ui_scale_y(MARGIN_BETWEEN_BORDER_AND_NAME_PERC)},
                        name_color, shadow_color, text_shadow_offset);

        // Responses
        BOOL const text_is_fully_printed = con->char_count_to_print == node->node_text->length;
        if (text_is_fully_printed) {
            for (SZ i = 0; i < node->response_count; ++i) {
                C8 const *response = node->responses[i].response_text->c;
                text_dimensions = measure_text(response_font, response);
                Vector2 const response_position = {
                    box_position.x + (box_size.x / 2.0F) - (text_dimensions.x / 2.0F),
                    box_position.y + MARGIN_BETWEEN_SENTENCE_AND_RESPONSES + (box_size.y / 2.0F - text_dimensions.y / 2.0F) +
                        (text_dimensions.y + MARGIN_BETWEEN_RESPONSES) + (text_dimensions.y * (F32)i) + (MARGIN_BETWEEN_RESPONSES * (F32)i),
                };

                BOOL const is_selected = node->response_selection == i;
                Color const color      = is_selected ? selected_response_color : response_color;

                // Skip this if there is only one response.
                if (is_selected && node->response_count > 1) {
                    F32 const horizontal_offset  = 2.5F;
                    F32 const animation_duration = 1.0F;  // Duration of one full cycle (up and down)
                    F32 const bobbing_amplitude  = 1.5F;  // Vertical movement amplitude in pixels

                    // Reset the timer once the duration is complete
                    if (talker->cursor_timer > animation_duration) { talker->cursor_timer -= animation_duration; }

                    // Calculate the bobbing offset using a sine wave for smooth oscillation
                    F32 const normalized_time = talker->cursor_timer / animation_duration;  // Normalized time (0 to 1)
                    F32 const bobbing_offset  = math_sin_f32(normalized_time * 2.0F * glm::pi<F32>()) * bobbing_amplitude;

                    // ASCII representation of the cursor
                    C8 const *cursor_text                = ">";
                    Vector2 const cursor_text_dimensions = measure_text(node_font, cursor_text);
                    Vector2 const cursor_text_position   = {
                        response_position.x - cursor_text_dimensions.x - horizontal_offset,
                        response_position.y + ((text_dimensions.y - cursor_text_dimensions.y) / 2.0F) + bobbing_offset + 4.0F,
                    };
                    // Draw the ASCII cursor with shadow
                    d2d_text_shadow(response_icon_font, cursor_text, cursor_text_position, cursor_color, shadow_color, text_shadow_offset);
                }

                d2d_text_shadow(response_font, response, response_position, color, shadow_color, text_shadow_offset);
            }
        }

        // Continue text
        if (node->response_count == 0) {  // This means we need to press continue.
            F32 static blink_timer = 0.0F;

            // Define the blink rate dynamically
            F32 const blinks_per_second = 1.0F;                             // Example: 2 blinks per second
            F32 const blink_frequency   = 1.0F / (2.0F * blinks_per_second);  // Calculate time for half a blink

            BOOL static should_draw = true;

            F32 const dt = time_get_delta();
            blink_timer += dt;

            // Blink logic
            if (blink_timer >= blink_frequency) {
                should_draw = !should_draw;  // Toggle drawing
                blink_timer = 0.0F;          // Reset the timer
            }

            if (should_draw) {
                C8 const *continue_text                = "...";
                Vector2 const continue_text_dimensions = measure_text(node_font, continue_text);
                Vector2 const continue_text_position   = {
                    box_position.x + box_size.x - continue_text_dimensions.x - 20.0F,
                    box_position.y + box_size.y - continue_text_dimensions.y - 20.0F,
                };

                d2d_text_shadow(node_font, continue_text, continue_text_position, continue_color, shadow_color, text_shadow_offset);
            }
        }
    }
}
