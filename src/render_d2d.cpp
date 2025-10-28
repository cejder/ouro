#include "color.hpp"
#include "cvar.hpp"
#include "loading.hpp"
#include "message.hpp"
#include "render.hpp"
#include "render_tooltip.hpp"
#include "std.hpp"
#include "string.hpp"
#include "time.hpp"
#include "world.hpp"

#include <raymath.h>

#define COLOR_TO_GLM_VEC4(col) {(F32)(col).r / 255.0F, (F32)(col).g / 255.0F, (F32)(col).b / 255.0F, (F32)(col).a / 255.0F}

void d2d_loading_screen(Loading *loading, C8 const *label) {
    AFont *font                    = g_render.default_font;
    F32 const max_width            = (F32)(S32)((F32)c_video__window_resolution_width * 0.15F);
    F32 const half_max_width       = max_width / 2.0F;
    F32 const height               = (F32)(S32)((F32)c_video__window_resolution_height * 0.01F);
    F32 const progress             = loading_get_progress_perc(loading);
    Color const bar_start_color    = RED;
    Color const bar_end_color      = GREEN;
    Color const bar_color          = Fade(color_lerp(bar_start_color, bar_end_color, progress), 0.75F);
    Color const border_color       = BLACK;
    Color const label_color        = NAYBEIGE;
    Color const label_shadow_color = BLACK;
    Vector2 const center           = render_get_center_of_window();
    Rectangle const bar_rec        = {center.x - half_max_width, (F32)c_video__window_resolution_height - (height * 2.5F), progress * max_width, height};
    Rectangle const bar_full_rec   = {bar_rec.x, bar_rec.y, max_width, bar_rec.height};
    Vector2 const shadow_offset    = {1.0F, 1.0F};
    Vector2 const label_dimensions = measure_text_ouc(font, label);
    Vector2 const label_position   = {center.x - (label_dimensions.x / 2.0F), bar_rec.y - (label_dimensions.y + 10.0F)};

    BeginDrawing();

    d2d_whole_screen(asset_get_texture("loading.jpg"));
    d2d_rectangle_rec(bar_full_rec, Fade(NAYBEIGE, 0.75F));
    d2d_rectangle_rec(bar_rec, bar_color);
    d2d_text_ouc_shadow(font, label, label_position, NAYBEIGE, NAYGREEN, shadow_offset);

    EndDrawing();
}

void d2d_text(AFont *font, C8 const *text, Vector2 position, Color tint) {
    INCREMENT_DRAW_CALL;

    position.x = (F32)((S32)position.x);
    position.y = (F32)((S32)position.y);
    DrawTextEx(font->base, text, position, (F32)font->font_size, 0.0F, tint);
}

void d2d_text_shadow(AFont *font, C8 const *text, Vector2 position, Color tint, Color shadow_color, Vector2 shadow_offset) {
    INCREMENT_DRAW_CALL;

    position.x = (F32)((S32)position.x);
    position.y = (F32)((S32)position.y);

    Vector2 const shadow_position = {position.x + shadow_offset.x, position.y + shadow_offset.y};
    DrawTextEx(font->base, text, shadow_position, (F32)font->font_size, 0.0F, shadow_color);
    DrawTextEx(font->base, text, position, (F32)font->font_size, 0.0F, tint);
}

#define OUC_PREFIX "\\ouc{"
#define OUC_PREFIX_LENGTH 5

void d2d_text_ouc(AFont *font, C8 const *text, Vector2 position, Color default_color) {
    INCREMENT_DRAW_CALL;

    Color current_color  = default_color;
    Vector2 current_pos  = position;
    C8 const *line_start = text;
    C8 const *newline    = nullptr;

    // Preallocate buffers once
    C8 line_buffer[OUC_MAX_SEGMENT_LENGTH];
    C8 segment[OUC_MAX_SEGMENT_LENGTH];

    while ((newline = ou_strchr(line_start, '\n')) != nullptr || *line_start) {
        // Get line length and copy to buffer
        SZ const line_length = newline ? (SZ)(newline - line_start) : ou_strlen(line_start);
        ou_memcpy(line_buffer, line_start, line_length);
        line_buffer[line_length] = '\0';

        // Process color codes within the current line
        C8 const *current_text = line_buffer;
        C8 const *split_pos    = nullptr;

        while ((split_pos = ou_strstr(current_text, OUC_PREFIX)) != nullptr) {
            // Handle text before escape code
            if (split_pos > current_text) {
                SZ const segment_length = (SZ)(split_pos - current_text);
                ou_memcpy(segment, current_text, segment_length);
                segment[segment_length] = '\0';
                d2d_text(font, segment, current_pos, current_color);
                current_pos.x += measure_text(font, segment).x;
            }

            // Process escape code
            C8 const *end_escape = ou_strchr(split_pos, '}');
            if (!end_escape) { break; }

            // Update color from escape code
            current_color = color_from_cstr(split_pos + OUC_PREFIX_LENGTH);
            current_text = end_escape + 1;
        }

        // Draw remaining text in line
        if (*current_text) { d2d_text(font, current_text, current_pos, current_color); }

        // Break if no more lines
        if (!newline) { break; }

        // Update position for next line
        current_pos.x  = position.x;
        current_pos.y += (F32)font->font_size;
        line_start     = newline + 1;
    }
}

void d2d_text_ouc_shadow(AFont *font, C8 const *text, Vector2 position, Color default_color, Color shadow_color, Vector2 shadow_offset) {
    INCREMENT_DRAW_CALL;

    Color current_color  = default_color;
    Vector2 current_pos  = position;
    C8 const *line_start = text;
    C8 const *newline    = nullptr;

    // Preallocate buffers once
    C8 line_buffer[OUC_MAX_SEGMENT_LENGTH];
    C8 segment[OUC_MAX_SEGMENT_LENGTH];

    while ((newline = ou_strchr(line_start, '\n')) != nullptr || *line_start) {
        // Get line length and copy to buffer
        SZ const line_length = newline ? (SZ)(newline - line_start) : ou_strlen(line_start);
        ou_memcpy(line_buffer, line_start, line_length);
        line_buffer[line_length] = '\0';

        // Process color codes within the current line
        C8 const *current_text = line_buffer;
        C8 const *split_pos = nullptr;

        while ((split_pos = ou_strstr(current_text, OUC_PREFIX)) != nullptr) {
            // Handle text before escape code
            if (split_pos > current_text) {
                SZ const segment_length = (SZ)(split_pos - current_text);
                ou_memcpy(segment, current_text, segment_length);
                segment[segment_length] = '\0';
                d2d_text_shadow(font, segment, current_pos, current_color, shadow_color, shadow_offset);
                current_pos.x += measure_text(font, segment).x;
            }

            // Process escape code
            C8 const *end_escape = ou_strchr(split_pos, '}');
            if (!end_escape) { break; }

            // Update color from escape code
            current_color = color_from_cstr(split_pos + OUC_PREFIX_LENGTH);
            current_text  = end_escape + 1;
        }

        // Draw remaining text in line
        if (*current_text) { d2d_text_shadow(font, current_text, current_pos, current_color, shadow_color, shadow_offset); }

        // Break if no more lines
        if (!newline) { break; }

        // Update position for next line
        current_pos.x  = position.x;
        current_pos.y += (F32)font->font_size;
        line_start     = newline + 1;
    }
}

void d2d_texture(ATexture *texture, F32 x, F32 y, Color tint) {
    INCREMENT_DRAW_CALL;

    DrawTexture(texture->base, (S32)x, (S32)y, tint);
}

void d2d_texture_raylib(Texture2D texture, F32 x, F32 y, Color tint) {
    INCREMENT_DRAW_CALL;

    DrawTexture(texture, (S32)x, (S32)y, tint);
}

void d2d_texture_ex(ATexture *texture, Vector2 position, F32 rotation, F32 scale, Color tint) {
    INCREMENT_DRAW_CALL;

    DrawTextureEx(texture->base, position, rotation, scale, tint);
}

void d2d_texture_ex_raylib(Texture2D texture, Vector2 position, F32 rotation, F32 scale, Color tint) {
    INCREMENT_DRAW_CALL;

    DrawTextureEx(texture, position, rotation, scale, tint);
}

void d2d_texture_pro(ATexture *texture, Rectangle src, Rectangle dst, Vector2 origin, F32 rotation, Color tint) {
    INCREMENT_DRAW_CALL;

    DrawTexturePro(texture->base, PIXEL_ALIGN_RECT(src), PIXEL_ALIGN_RECT(dst), origin, rotation, tint);
}

void d2d_texture_pro_raylib(Texture2D texture, Rectangle src, Rectangle dst, Vector2 origin, F32 rotation, Color tint) {
    INCREMENT_DRAW_CALL;

    DrawTexturePro(texture, PIXEL_ALIGN_RECT(src), PIXEL_ALIGN_RECT(dst), origin, rotation, tint);
}

void d2d_texture_v(ATexture *texture, Vector2 position, Color tint) {
    INCREMENT_DRAW_CALL;

    DrawTextureV(texture->base, position, tint);
}

void d2d_texture_rec(ATexture *texture, Rectangle src, Vector2 position, Color tint) {
    INCREMENT_DRAW_CALL;

    DrawTextureRec(texture->base, PIXEL_ALIGN_RECT(src), position, tint);
}

void d2d_texture_rec_raylib(Texture2D texture, Rectangle src, Vector2 position, Color tint) {
    INCREMENT_DRAW_CALL;

    DrawTextureRec(texture, PIXEL_ALIGN_RECT(src), position, tint);
}

void d2d_texture_gl(U32 id, Vector2 position, Vector2 size, Color tint) {
    INCREMENT_DRAW_CALL;

    DrawTexture({id, (S32)size.x, (S32)size.y, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8}, (S32)position.x, (S32)position.y, tint);
}

void d2d_triangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color) {
    INCREMENT_DRAW_CALL;

    DrawTriangle(v1, v2, v3, color);
}

void d2d_rectangle(S32 x, S32 y, S32 width, S32 height, Color color) {
    INCREMENT_DRAW_CALL;

    DrawRectangle(x, y, width, height, color);
}

void d2d_rectangle_lines(S32 x, S32 y, S32 width, S32 height, Color color) {
    INCREMENT_DRAW_CALL;

    DrawRectangleLines(x, y, width, height, color);
}

void d2d_rectangle_lines_ex(Rectangle rec, F32 line_thickness, Color color) {
    INCREMENT_DRAW_CALL;

    DrawRectangleLinesEx(PIXEL_ALIGN_RECT(rec), (F32)(S32)line_thickness, color);
}

void d2d_rectangle_v(Vector2 position, Vector2 size, Color color) {
    INCREMENT_DRAW_CALL;

    DrawRectangleV(position, size, color);
}

void d2d_rectangle_rec(Rectangle rec, Color color) {
    INCREMENT_DRAW_CALL;

    DrawRectangle((S32)rec.x, (S32)rec.y, (S32)rec.width, (S32)rec.height, color);
}

void d2d_rectangle_rec_lines(Rectangle rec, Color color) {
    INCREMENT_DRAW_CALL;

    d2d_rectangle_lines((S32)rec.x, (S32)rec.y, (S32)rec.width, (S32)rec.height, color);
}

void d2d_rectangle_rounded(S32 x, S32 y, S32 width, S32 height, F32 roundness, S32 segments, Color color) {
    INCREMENT_DRAW_CALL;

    DrawRectangleRounded(Rectangle{(F32)x, (F32)y, (F32)width, (F32)height}, roundness, segments, color);
}

void d2d_rectangle_rounded_lines(S32 x, S32 y, S32 width, S32 height, F32 roundness, S32 segments, Color color) {
    INCREMENT_DRAW_CALL;

    DrawRectangleRoundedLines(Rectangle{(F32)x, (F32)y, (F32)width, (F32)height}, roundness, segments, color);
}

void d2d_rectangle_rounded_lines_ex(Rectangle rec, F32 roundness, S32 segments, F32 line_thickness, Color color) {
    INCREMENT_DRAW_CALL;

    DrawRectangleRoundedLinesEx(PIXEL_ALIGN_RECT(rec), roundness, segments, line_thickness, color);
}

void d2d_rectangle_rounded_v(Vector2 position, Vector2 size, F32 roundness, S32 segments, Color color) {
    INCREMENT_DRAW_CALL;

    Rectangle const rec = {(F32)(S32)position.x, (F32)(S32)position.y, (F32)(S32)size.x, (F32)(S32)size.y};
    DrawRectangleRounded(rec, roundness, segments, color);
}

void d2d_rectangle_rounded_rec(Rectangle rec, F32 roundness, S32 segments, Color color) {
    INCREMENT_DRAW_CALL;

    DrawRectangleRounded(PIXEL_ALIGN_RECT(rec), roundness, segments, color);
}

void d2d_rectangle_rounded_rec_lines(Rectangle rec, F32 roundness, S32 segments, Color color) {
    INCREMENT_DRAW_CALL;

    DrawRectangleRoundedLines(PIXEL_ALIGN_RECT(rec), roundness, segments, color);
}

void d2d_line_strip(Vector2 *points, SZ point_count, Color color) {
    INCREMENT_DRAW_CALL;

    DrawLineStrip(points, (S32)point_count, color);
}

void d2d_line(Vector2 start, Vector2 end, Color color) {
    INCREMENT_DRAW_CALL;

    DrawLine((S32)start.x, (S32)start.y, (S32)end.x, (S32)end.y, color);
}

void d2d_line_ex(Vector2 start, Vector2 end, F32 thick, Color color) {
    INCREMENT_DRAW_CALL;

    DrawLineEx(start, end, thick, color);
}

void d2d_circle(Vector2 center, F32 radius, Color color) {
    INCREMENT_DRAW_CALL;

    DrawCircle((S32)center.x, (S32)center.y, radius, color);
}

void d2d_circle_lines(Vector2 center, F32 radius, Color color) {
    INCREMENT_DRAW_CALL;

    DrawCircleLines((S32)center.x, (S32)center.y, radius, color);
}

void d2d_circle_sector(Vector2 center, F32 radius, F32 start_angle, F32 end_angle, S32 segments, Color color) {
    INCREMENT_DRAW_CALL;

    DrawCircleSector(center, radius, start_angle, end_angle, segments, color);
}

void d2d_circle_sector_lines(Vector2 center, F32 radius, F32 start_angle, F32 end_angle, S32 segments, Color color) {
    INCREMENT_DRAW_CALL;

    DrawCircleSectorLines(center, radius, start_angle, end_angle, segments, color);
}

void d2d_whole_screen(ATexture *texture) {
    INCREMENT_DRAW_CALL;

    d2d_texture_pro(texture, {0.0F, 0.0F, (F32)texture->base.width, (F32)texture->base.height},
                    {0.0F, 0.0F, (F32)c_video__window_resolution_width, (F32)c_video__window_resolution_height},
                    {}, 0.0F, WHITE);
}

void d2d_gizmo(EID id) {
    AFont *font = g_render.default_font;

    Vector3 const position      = g_world->position[id];
    F32 const rotation          = g_world->rotation[id];
    F32 const cam_dist          = Vector3Distance(position, g_render.cameras.c3d.active_cam->position);
    AModel *model               = asset_get_model(g_world->model_name[id]);
    C8 tint_hex[COLOR_HEX_SIZE] = {};
    color_hex_from_color(g_world->tint[id], tint_hex);

    RenderTooltip tt = {};
    render_tooltip_init(font, &tt, TS("  "), position, true);

    // Entity
    rib_ENTITY(&tt, "\\ouc{#aaf33fff}E_\\ouc{#ffffffff}SELF", g_world, id);
    rib_F32(&tt,    "\\ouc{#aaf33fff}E_\\ouc{#ffffffff}LT", g_world->lifetime[id]);

    // Health
    rib_HEALTH(&tt, "\\ouc{#fff33fff}H_\\ouc{#ffffffff}MAX", g_world->health[id].current, g_world->health[id].max);

    // Transform - Show both stored and actual values
    rib_V3(&tt,  "\\ouc{#ff99ffff}T_\\ouc{#ffffffff}POS", position);
    rib_V3(&tt,  "\\ouc{#ff99ffff}T_\\ouc{#ffffffff}SCALE", g_world->scale[id]);
    rib_F32(&tt, "\\ouc{#ff99ffff}T_\\ouc{#ffffffff}ROT", rotation);

    // Collision/OBB Information (renamed to avoid confusion with physics)
    rib_V3(&tt, "\\ouc{#ff3273ff}C_\\ouc{#ffffffff}CENTER", g_world->obb[id].center);
    rib_V3(&tt, "\\ouc{#ff3273ff}C_\\ouc{#ffffffff}EXTENTS", g_world->obb[id].extents);
    rib_V3(&tt, "\\ouc{#ff3273ff}C_\\ouc{#ffffffff}AXES", g_world->obb[id].axes[0]);

    // Rendering
    rib_F32(&tt,   "\\ouc{#00bfffff}R_\\ouc{#ffffffff}DIST", cam_dist);
    rib_MODEL(&tt, "\\ouc{#00bfffff}R_\\ouc{#ffffffff}MODEL", model->header.name, model->vertex_count);
    rib_COLOR(&tt, "\\ouc{#00bfffff}R_\\ouc{#ffffffff}TINT", tint_hex, g_world->tint[id]);

    // Animation (if entity has animations)
    if (g_world->animation[id].has_animations) {
        rib_STR(&tt,  "\\ouc{#00ff7fff}A_\\ouc{#ffffffff}PLAYING", g_world->animation[id].anim_playing ? "TRUE" : "FALSE");
        rib_U32(&tt,  "\\ouc{#00ff7fff}A_\\ouc{#ffffffff}INDEX", g_world->animation[id].anim_index);
        rib_U32(&tt,  "\\ouc{#00ff7fff}A_\\ouc{#ffffffff}FRAME", g_world->animation[id].anim_frame);
        rib_F32(&tt,  "\\ouc{#00ff7fff}A_\\ouc{#ffffffff}TIME", g_world->animation[id].anim_time);
        rib_F32(&tt,  "\\ouc{#00ff7fff}A_\\ouc{#ffffffff}SPEED", g_world->animation[id].anim_speed);
        rib_F32(&tt,  "\\ouc{#00ff7fff}A_\\ouc{#ffffffff}FPS", g_world->animation[id].anim_fps);
        rib_STR(&tt,  "\\ouc{#00ff7fff}A_\\ouc{#ffffffff}LOOP", g_world->animation[id].anim_loop ? "TRUE" : "FALSE");
        rib_S32(&tt,  "\\ouc{#00ff7fff}A_\\ouc{#ffffffff}BONES", g_world->animation[id].bone_count);
    }

    // Behavior
    if (ENTITY_HAS_FLAG(g_world->flags[id], ENTITY_FLAG_ACTOR)) {
        EntityActor *actor                 = &g_world->actor[id];
        EntityBehaviorController *behavior = &actor->behavior;
        EntityMovementController *movement = &actor->movement;

        rib_STR(&tt, "\\ouc{#9d4edeff}B_\\ouc{#ffffffff}STATE", entity_actor_behavior_state_to_cstr(behavior->state));
        rib_STR(&tt, "\\ouc{#9d4edeff}B_\\ouc{#ffffffff}TARGET_TYPE", entity_type_to_cstr(behavior->target_entity_type));

        if (behavior->target_id != INVALID_EID) {
            rib_U32(&tt, "\\ouc{#9d4edeff}B_\\ouc{#ffffffff}TARGET_ID", behavior->target_id);
            rib_U32(&tt, "\\ouc{#9d4edeff}B_\\ouc{#ffffffff}TARGET_GEN", behavior->target_gen);
        }

        BOOL const has_target = (behavior->target_id != INVALID_EID || behavior->target_position.x != 0.0F || behavior->target_position.z != 0.0F);
        if (has_target) {
            rib_V3(&tt,  "\\ouc{#9d4edeff}B_\\ouc{#ffffffff}TARGET_POS", behavior->target_position);
            rib_F32(&tt, "\\ouc{#9d4edeff}B_\\ouc{#ffffffff}TARGET_DIST", Vector3Distance(position, behavior->target_position));
        }

        // Movement
        rib_STR(&tt, "\\ouc{#ff8c42ff}M_\\ouc{#ffffffff}STATE", entity_actor_movement_state_to_cstr(movement->state));
        rib_STR(&tt, "\\ouc{#ff8c42ff}M_\\ouc{#ffffffff}GOAL", entity_actor_movement_goal_to_cstr(movement->goal_type));
        rib_F32(&tt, "\\ouc{#ff8c42ff}M_\\ouc{#ffffffff}SPEED", movement->current_speed);

        if (movement->goal_completed) {
            rib_STR(&tt, "\\ouc{#ff8c42ff}M_\\ouc{#ffffffff}STATUS", "\\ouc{#00ff00ff}COMPLETED");
        } else if (movement->goal_failed) {
            rib_STR(&tt, "\\ouc{#ff8c42ff}M_\\ouc{#ffffffff}STATUS", "\\ouc{#ff0000ff}FAILED");
        }

        rib_F32(&tt, "\\ouc{#ff8c42ff}M_\\ouc{#ffffffff}STUCK_TIME", movement->stuck_timer);
        rib_V3(&tt, "\\ouc{#ff8c42ff}M_\\ouc{#ffffffff}SEPARATION", movement->separation_force);

        // Actions
        if (behavior->wood_count > 0) { rib_SZ(&tt, "\\ouc{#e74c3cff}A_\\ouc{#ffffffff}WOOD", behavior->wood_count); }

        if (behavior->action_timer > 0.0F) { rib_F32(&tt, "\\ouc{#e74c3cff}A_\\ouc{#ffffffff}TIME_LEFT", behavior->action_timer); }
    }

    // Structure
    if (g_world->type[id] == ENTITY_TYPE_BUILDING_LUMBERYARD) {
        rib_SZ(&tt, "\\ouc{#27ae60ff}S_\\ouc{#ffffffff}WOOD", g_world->building[id].lumberyard.wood_count);
    }

    render_tooltip_draw(&tt);
}

void d2d_healthbar(EID id) {
    if (!c_world__actor_healthbar)                                    { return; }
    if (g_world->health[id].max <= 0)                                 { return; }
    if (!ENTITY_HAS_FLAG(g_world->flags[id], ENTITY_FLAG_IN_FRUSTUM)) { return; }

    Camera3D *cam                 = c3d_get_ptr();
    Vector2 const render_res      = render_get_render_resolution();
    Vector3 const position        = g_world->position[id];
    OrientedBoundingBox const obb = g_world->obb[id];

    Vector3 const world_pos  = {position.x, position.y + (obb.extents.y * 2.8F), position.z};
    Vector2 const screen_pos = GetWorldToScreenEx(world_pos, *cam, (S32)render_res.x, (S32)render_res.y);

    if (screen_pos.x < -150 || screen_pos.x > render_res.x + 150 || screen_pos.y < -150 || screen_pos.y > render_res.y + 150) { return; }

    F32 distance = Vector3Distance(position, cam->position);
    if (g_world->selected_id == id) { distance = 0.0F; } // If this is the selected entity, we do not care about distance.
    F32 const max_distance = 10.0F + g_world->radius[id];
    F32 const fade_start   = max_distance * 0.75F;

    if (distance > max_distance) { return; }

    F32 const distance_t = glm::clamp((distance - fade_start) / (max_distance - fade_start), 0.0F, 1.0F);
    F32 const alpha      = ease_out_quart(1.0F - distance_t, 0.0F, 1.0F, 1.0F);

    F32 const health_perc  = glm::clamp((F32)g_world->health[id].current / (F32)g_world->health[id].max, 0.0F, 1.0F);
    F32 const fill_width_t = ease_out_expo(health_perc, 0.0F, 1.0F, 1.0F);

    F32 const bar_width    = ui_scale_x(8.0F);
    F32 const bar_height   = ui_scale_y(2.30F);
    F32 const border_thick = ui_scale_x(0.20F);
    F32 const roundness    = 0.5F;
    S32 const segments     = 10;

    Color base_color;
    if (health_perc > 0.7F) {
        base_color = SPRINGLEAF;
    } else if (health_perc > 0.4F) {
        F32 const t = (health_perc - 0.4F) / 0.3F;
        base_color  = color_lerp(SUNSETAMBER, SPRINGLEAF, ease_in_out_cubic(t, 0.0F, 1.0F, 1.0F));
    } else if (health_perc > 0.15F) {
        F32 const t = (health_perc - 0.15F) / 0.25F;
        base_color  = color_lerp(PERSIMMON, SUNSETAMBER, ease_in_out_cubic(t, 0.0F, 1.0F, 1.0F));
    } else {
        F32 const pulse             = ease_in_out_sine((math_sin_f32(time_get() * 8.0F) + 1.0F) * 0.5F, 0.0F, 1.0F, 1.0F);
        Color const critical_base   = color_saturated(PERSIMMON);
        Color const critical_bright = color_blend(PERSIMMON, TANGERINE);
        base_color                  = color_lerp(critical_base, critical_bright, pulse * 0.7F);
    }

    Color bg_color     = Fade(NEARBLACK, 0.88F * alpha);
    Color border_color = Fade(BEIGE, 0.20F * alpha);
    Color fill_color   = Fade(base_color, alpha);

    Vector2 const bar_pos       = {screen_pos.x - (bar_width * 0.5F), screen_pos.y - (bar_height * 0.5F)};
    Rectangle const bg_rect     = {bar_pos.x, bar_pos.y, bar_width, bar_height};
    Rectangle const fill_rect   = {bar_pos.x, bar_pos.y, bar_width * fill_width_t, bar_height};
    Rectangle const shadow_rect = {bg_rect.x + 1.5F, bg_rect.y + 1.5F, bg_rect.width, bg_rect.height};

    d2d_rectangle_rounded_rec(shadow_rect, roundness, segments, Fade(BLACK, 0.25F * alpha));
    d2d_rectangle_rounded_rec(bg_rect, roundness, segments, bg_color);

    if (fill_width_t > 0.01F) { d2d_rectangle_rounded_rec(fill_rect, roundness, segments, fill_color); }

    d2d_rectangle_rounded_lines_ex(bg_rect, roundness, segments, border_thick, border_color);

    AFont *name_font        = asset_get_font("GoMono", ui_font_size(2.0F));
    Vector2 const name_size = measure_text(name_font, g_world->name[id]);
    Vector2 const name_pos  = {screen_pos.x - (name_size.x * 0.5F), bar_pos.y + (bar_height * 0.5F) - (name_size.y * 0.5F)};

    Color const text_color      = RAYWHITE;
    Color const shadow_color    = DARKBROWN;
    Vector2 const shadow_offset = ui_shadow_offset(0.05F, 0.075F);

    d2d_text_shadow(name_font, g_world->name[id], name_pos, Fade(text_color, alpha), Fade(shadow_color, 0.8F * alpha), shadow_offset);

    SZ const wood_count = g_world->actor[id].behavior.wood_count;
    if (wood_count > 0) {
        Texture2D icon         = asset_get_model("wood.glb")->icon;
        F32 const icon_size    = ui_scale_x(2.0F);
        F32 const tilt_angle   = 0.0F;
        Vector2 const icon_pos = {bar_pos.x + bar_width + ui_scale_x(0.75F), bar_pos.y - (icon_size/2)};

        d2d_texture_ex_raylib(icon, icon_pos, tilt_angle, icon_size / (F32)icon.width, Fade(SUNSETAMBER, alpha));

        AFont *count_font            = asset_get_font("GoMono", ui_font_size(2.5F));
        String const *count_text     = TS("%zu", wood_count);
        Vector2 const count_pos      = {icon_pos.x + (icon_size/3), icon_pos.y + (icon_size/2)};
        Color const big_text_color   = WHITE;
        Color const big_shadow_color = NEARBLACK;
        Vector2 const big_shadow     = ui_shadow_offset(0.05F, 0.075F);

        d2d_text_shadow(count_font, count_text->c, count_pos, Fade(big_text_color, alpha), Fade(big_shadow_color, 0.9F * alpha), big_shadow);
    }
}

void d2d_bone_gizmo(EID id) {
    if (!g_world->animation[id].has_animations) { return; }

    AModel *model = asset_get_model(g_world->model_name[id]);
    if (!model || !model->animations) { return; }
    if (!model->base.bones) { return; }

    U32 const anim_idx = g_world->animation[id].anim_index;
    U32 const frame    = g_world->animation[id].anim_frame;

    if (anim_idx >= (U32)model->animation_count) { return; }

    ModelAnimation const& anim = model->animations[anim_idx];
    if (frame >= (U32)anim.frameCount) { return; }

    S32 const bone_count = g_world->animation[id].bone_count;
    AFont *font          = asset_get_font(c_debug__medium_font.cstr, c_debug__bone_label_font_size);

    // Build entity transform matrix (same as d3d_model_animated)
    Vector3 const position = g_world->position[id];
    Vector3 const scale    = g_world->scale[id];
    F32 const rotation     = g_world->rotation[id];

    Matrix mat_scale    = MatrixScale(scale.x, scale.y, scale.z);
    Matrix mat_rotation = MatrixRotate((Vector3){0, 1, 0}, rotation * DEG2RAD);
    Matrix mat_position = MatrixTranslate(position.x, position.y, position.z);
    Matrix entity_transform = MatrixMultiply(MatrixMultiply(mat_scale, mat_rotation), mat_position);

    // Compute world-space bone matrices by accumulating through hierarchy
    Matrix bone_world_matrices[ENTITY_MAX_BONES];
    for (S32 bone_idx = 0; bone_idx < bone_count; bone_idx++) {
        Transform *bone_transform = &anim.framePoses[frame][bone_idx];

        // Build local bone matrix
        Matrix local_matrix = MatrixMultiply(MatrixMultiply(
            MatrixScale(bone_transform->scale.x, bone_transform->scale.y, bone_transform->scale.z),
            QuaternionToMatrix(bone_transform->rotation)),
            MatrixTranslate(bone_transform->translation.x, bone_transform->translation.y, bone_transform->translation.z));

        // Accumulate parent transforms (parent * local for proper hierarchy)
        S32 parent_idx = model->base.bones[bone_idx].parent;
        if (parent_idx >= 0) {
            bone_world_matrices[bone_idx] = MatrixMultiply(bone_world_matrices[parent_idx], local_matrix);
        } else {
            bone_world_matrices[bone_idx] = local_matrix;
        }
    }

    // Draw text labels for each bone
    for (S32 bone_idx = 0; bone_idx < bone_count; bone_idx++) {
        // Transform bone position to world space using entity transform
        Matrix final_bone_matrix = MatrixMultiply(bone_world_matrices[bone_idx], entity_transform);
        Vector3 world_pos = {final_bone_matrix.m12, final_bone_matrix.m13, final_bone_matrix.m14};

        // Project to screen
        Vector2 screen_pos = GetWorldToScreen(world_pos, *c3d_get_ptr());

        // Get bone name
        C8 const *bone_name = model->base.bones[bone_idx].name;

        // Format text: "[index] BoneName"
        C8 bone_label[128];
        ou_snprintf(bone_label, sizeof(bone_label), "[%d] %s", bone_idx, bone_name);

        // Draw text with shadow
        d2d_text_shadow(font, bone_label, screen_pos, NAYGREEN, NAYBEIGE, {1, 1});
    }
}
