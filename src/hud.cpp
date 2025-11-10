#include "hud.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "message.hpp"
#include "player.hpp"
#include "profiler.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "string.hpp"
#include "world.hpp"

HUD g_hud = {};

void hud_init(F32 top_perc, F32 bottom_perc, F32 left_perc, F32 right_perc) {
    g_hud.top_height_perc    = top_perc / 100.0F;  // Expecting actual percentage values like 10%.
    g_hud.bottom_height_perc = bottom_perc / 100.0F;
    g_hud.left_width_perc    = left_perc / 100.0F;
    g_hud.right_width_perc   = right_perc / 100.0F;
}

void hud_update(F32 dt, F32 dtu) {
    if (!c_render__hud) { return; }

    unused(dt);
    unused(dtu);
}

void hud_draw_2d_hud() {
    if (!c_render__hud) { return; }

    // PLAYERINFO

    // Font setup
    AFont *title_font = asset_get_font("GoMono", ui_font_size(2.0F));
    AFont *stat_font  = asset_get_font("GoMono", ui_font_size(1.2F));

    // Colors
    Color const text_color   = g_render.sketch_shader.minor_color;
    Color const shadow_color = Fade(NAYBEIGE, 0.8F);
    Color const status_color = color_sync_blinking_regular({0, 175, 0, 255}, {0, 255, 0, 255});

    Color const empty_bar_color = NEARBLACK;
    Color const anima_color     = EVAFATPURPLE;
    Color const sanctus_color   = EVAFATGREEN;
    Color const gnosis_color    = EVAFATORANGE;

    Color const status_bg_color = {(U8)(status_color.r / 2), (U8)(status_color.g / 2), (U8)(status_color.b / 2), 255};

    Vector2 const shadow_offset = ui_shadow_offset(0.05F, 0.075F);
    F32 const title_padding     = ui_scale_y(0.25);
    F32 const section_spacing   = ui_scale_y(-1.0);
    F32 const bar_height        = (F32)stat_font->font_size * 0.5F;
    F32 const bar_top_margin    = (F32)stat_font->font_size * 0.975F;
    F32 const bar_bottom_margin = ui_scale_y(0.20F);

    F32 const bar_roundness = 0.35F;
    S32 const bar_segments  = 8;

    // Initial positioning
    F32 const start_x   = ui_scale_x(32.0F);    // Horizontal starting position
    F32 current_y       = ui_scale_y(89.0F);        // Vertical starting position that will accumulate
    F32 const max_width = ui_scale_x(36.0F);  // Maximum width for elements

    // Mock player stats - these would normally come from player state
    S32 const ascension             = 12;
    S32 const anima                 = 78;
    S32 const max_anima             = 100;
    S32 const sanctus               = 45;
    S32 const max_sanctus           = 60;
    S32 const gnosis                = 3240;
    S32 const next_ascension_gnosis = 5000;
    F32 const gnosis_percent        = (F32)gnosis / (F32)next_ascension_gnosis;
    C8 const *status                = "HEALTHY";
    C8 const *class_name            = "TEMPLAR";

    // Draw title with clean centering
    C8 const *title_text     = "THORNE";
    Vector2 const title_size = measure_text(title_font, title_text);
    F32 const title_x        = start_x + ((max_width - title_size.x) * 0.5F);
    d2d_text_shadow(title_font, title_text, {title_x, current_y}, text_color, shadow_color, shadow_offset);
    current_y += title_size.y;

    // Draw underline
    F32 const underline_y = current_y;
    d2d_line_ex({title_x, underline_y}, {title_x + title_size.x, underline_y}, 2.0F, text_color);
    d2d_line_ex({title_x + shadow_offset.x, underline_y + shadow_offset.y}, {title_x + title_size.x + shadow_offset.x, underline_y + shadow_offset.y}, 2.0F, shadow_color);
    current_y = underline_y + title_padding;
    // Character class and ascension as a sentence
    C8 const *ordinal_suffix           = math_ordinal_suffix(ascension);
    String const *class_ascension_text = TS("%s of %d%s ASCENSION", class_name, ascension, ordinal_suffix);
    Vector2 const class_ascension_size = measure_text(stat_font, class_ascension_text->c);
    F32 const class_ascension_x        = start_x + ((max_width - class_ascension_size.x) * 0.5F);
    d2d_text_shadow(stat_font, class_ascension_text->c, {class_ascension_x, current_y}, text_color, shadow_color, shadow_offset);
    current_y += class_ascension_size.y + section_spacing;

    // ANIMA
    // Text
    d2d_text_shadow(stat_font, "ANIMA:", {start_x, current_y}, text_color, shadow_color, shadow_offset);
    String const *anima_text      = TS("%d/%d", anima, max_anima);
    Vector2 const anima_text_size = measure_text(stat_font, anima_text->c);
    F32 const anima_value_x       = start_x + max_width - anima_text_size.x;
    d2d_text_shadow(stat_font, anima_text->c, {anima_value_x, current_y}, text_color, shadow_color, shadow_offset);
    current_y += bar_top_margin;
    // Bar
    F32 const anima_percent      = (F32)anima / (F32)max_anima;
    Rectangle const anima_bg_rec = {start_x, current_y, max_width, bar_height};
    d2d_rectangle_rounded_rec(anima_bg_rec, bar_roundness, bar_segments, empty_bar_color);
    Rectangle const anima_fill_rec = {anima_bg_rec.x, anima_bg_rec.y, anima_bg_rec.width * anima_percent, anima_bg_rec.height};
    d2d_rectangle_rounded_rec(anima_fill_rec, bar_roundness, bar_segments, anima_color);
    // Bar segments
    for (SZ i = 1; i < 5; ++i) {
        F32 const x_pos = anima_bg_rec.x + (anima_bg_rec.width * ((F32)i / 5.0F));
        d2d_line_ex({x_pos, anima_bg_rec.y}, {x_pos, anima_bg_rec.y + anima_bg_rec.height}, 1.0F, {0, 0, 0, 100});
    }
    current_y += bar_height + bar_bottom_margin;

    // SANCTUS
    // Text
    d2d_text_shadow(stat_font, "SANCTUS:", {start_x, current_y}, text_color, shadow_color, shadow_offset);
    String const *sanctus_text      = TS("%d/%d", sanctus, max_sanctus);
    Vector2 const sanctus_text_size = measure_text(stat_font, sanctus_text->c);
    F32 const sanctus_value_x       = start_x + max_width - sanctus_text_size.x;
    d2d_text_shadow(stat_font, sanctus_text->c, {sanctus_value_x, current_y}, text_color, shadow_color, shadow_offset);
    current_y += bar_top_margin;
    // Bar
    F32 const sanctus_percent      = (F32)sanctus / (F32)max_sanctus;
    Rectangle const sanctus_bg_rec = {start_x, current_y, max_width, bar_height};
    d2d_rectangle_rounded_rec(sanctus_bg_rec, bar_roundness, bar_segments, empty_bar_color);
    Rectangle const sanctus_fill_rec = {sanctus_bg_rec.x, sanctus_bg_rec.y, sanctus_bg_rec.width * sanctus_percent, sanctus_bg_rec.height};
    d2d_rectangle_rounded_rec(sanctus_fill_rec, bar_roundness, bar_segments, sanctus_color);
    for (SZ i = 1; i < 5; ++i) {
        F32 const x_pos = sanctus_bg_rec.x + (sanctus_bg_rec.width * ((F32)i / 5.0F));
        d2d_line_ex({x_pos, sanctus_bg_rec.y}, {x_pos, sanctus_bg_rec.y + sanctus_bg_rec.height}, 1.0F, {0, 0, 0, 100});
    }
    current_y += bar_height + bar_bottom_margin;

    // GNOSIS
    // Text
    d2d_text_shadow(stat_font, "GNOSIS:", {start_x, current_y}, text_color, shadow_color, shadow_offset);
    String const *gnosis_text      = TS("%d/%d", gnosis, next_ascension_gnosis);
    Vector2 const gnosis_text_size = measure_text(stat_font, gnosis_text->c);
    F32 const gnosis_value_x       = start_x + max_width - gnosis_text_size.x;
    d2d_text_shadow(stat_font, gnosis_text->c, {gnosis_value_x, current_y}, text_color, shadow_color, shadow_offset);
    current_y += bar_top_margin;
    // Bar
    Rectangle const gnosis_bg_rec = {start_x, current_y, max_width, bar_height};
    d2d_rectangle_rounded_rec(gnosis_bg_rec, bar_roundness, bar_segments, empty_bar_color);
    Rectangle const gnosis_fill_rec = {gnosis_bg_rec.x, gnosis_bg_rec.y, gnosis_bg_rec.width * gnosis_percent, gnosis_bg_rec.height};
    d2d_rectangle_rounded_rec(gnosis_fill_rec, bar_roundness, bar_segments, gnosis_color);
    for (SZ i = 1; i < 10; ++i) {  // More segments for Gnosis bar
        F32 const x_pos = gnosis_bg_rec.x + (gnosis_bg_rec.width * ((F32)i / 10.0F));
        d2d_line_ex({x_pos, gnosis_bg_rec.y}, {x_pos, gnosis_bg_rec.y + gnosis_bg_rec.height}, 1.0F, {0, 0, 0, 100});
    }
    current_y += (bar_height + bar_bottom_margin) + ui_scale_x(0.20F);

    // Status
    d2d_text_shadow(stat_font, "STATUS:", {start_x, current_y}, text_color, shadow_color, shadow_offset);
    Vector2 const status_text_size = measure_text(stat_font, status);
    F32 const padding              = ui_scale_x(0.175F);
    F32 const status_value_x       = start_x + max_width - status_text_size.x - padding;
    // Status badge - rectangle with perfectly centered text
    Rectangle const status_bg = {
        status_value_x - padding,
        current_y - padding,
        status_text_size.x + (padding * 2.0F),
        status_text_size.y + (padding * 2.0F),
    };
    d2d_rectangle_rec(status_bg, status_bg_color);
    d2d_rectangle_lines_ex(status_bg, 1.0F, status_color);
    // Calculate exact center point of the rectangle
    F32 const center_rec_x = status_bg.x + (status_bg.width / 2.0F);
    F32 const center_rec_y = status_bg.y + (status_bg.height / 2.0F);
    // Position text at the exact center of the rectangle
    F32 const text_x = center_rec_x - (status_text_size.x / 2.0F);
    F32 const text_y = center_rec_y - (status_text_size.y / 2.0F);
    d2d_text_shadow(stat_font, status, {text_x, text_y}, status_color, BLACK, shadow_offset);

    // HACK: TOTAL WOOD
    SZ total_wood = 0;
    for (SZ i = 0; i < WORLD_MAX_ENTITIES; ++i) {
        if (g_world->type[i] == ENTITY_TYPE_BUILDING_LUMBERYARD) { total_wood += g_world->building[i].lumberyard.wood_count; }
    }

    AFont *font = asset_get_font("GoMono", ui_font_size(3));
    C8 const *text = TS("Wood: \\ouc{#335272ff}%zu", total_wood)->c;
    Vector2 text_dim = measure_text_ouc(font, text);
    d2d_text_ouc_shadow(font, text, {
        ((F32)c_video__render_resolution_width/2) - (text_dim.x/2),
        (ui_scale_y(g_hud.top_height_perc * 100)/2) - (text_dim.y/2),
    }, text_color, shadow_color, shadow_offset);
};

void hud_draw_2d_hud_sketch() {
    if (!c_render__hud) { return; }

    Color const hud_tint = g_render.sketch_shader.major_color;
    Vector2 const res = {(F32)c_video__render_resolution_width, (F32)c_video__render_resolution_height};

    // BACKGROUND

    // Top border (full width)
    if (g_hud.top_height_perc > 0.0F) {
        ATexture *top_hud       = asset_get_texture("hud_top.png");
        Rectangle const top_src = {0.0F, 0.0F, (F32)top_hud->base.width, (F32)top_hud->base.height};
        Rectangle const top_dst = {0.0F, 0.0F, res.x, res.y * g_hud.top_height_perc};
        d2d_texture_pro(top_hud, top_src, top_dst, {0.0F, 0.0F}, 0.0F, hud_tint);
    }

    // Bottom border (full width)
    if (g_hud.bottom_height_perc > 0.0F) {
        ATexture *bottom_hud       = asset_get_texture("hud_bottom.png");
        Rectangle const bottom_src = {0.0F, 0.0F, (F32)bottom_hud->base.width, (F32)bottom_hud->base.height};
        Rectangle const bottom_dst = {0.0F, res.y * (1.0F - g_hud.bottom_height_perc), res.x, res.y * g_hud.bottom_height_perc};
        d2d_texture_pro(bottom_hud, bottom_src, bottom_dst, {0.0F, 0.0F}, 0.0F, hud_tint);
    }

    // Left border (center only - avoiding top and bottom overlap)
    if (g_hud.left_width_perc > 0.0F) {
        ATexture *left_hud       = asset_get_texture("hud_left.png");
        Rectangle const left_src = {0.0F, 0.0F, (F32)left_hud->base.width, (F32)left_hud->base.height};
        Rectangle const left_dst = {0.0F, res.y * g_hud.top_height_perc, res.x * g_hud.left_width_perc,
                                    res.y * (1.0F - g_hud.top_height_perc - g_hud.bottom_height_perc)};
        d2d_texture_pro(left_hud, left_src, left_dst, {0.0F, 0.0F}, 0.0F, hud_tint);
    }

    // Right border (center only - avoiding top and bottom overlap)
    if (g_hud.right_width_perc > 0.0F) {
        ATexture *right_hud       = asset_get_texture("hud_right.png");
        Rectangle const right_src = {0.0F, 0.0F, (F32)right_hud->base.width, (F32)right_hud->base.height};
        Rectangle const right_dst = {res.x * (1.0F - g_hud.right_width_perc), res.y * g_hud.top_height_perc, res.x * g_hud.right_width_perc,
                                     res.y * (1.0F - g_hud.top_height_perc - g_hud.bottom_height_perc)};
        d2d_texture_pro(right_hud, right_src, right_dst, {0.0F, 0.0F}, 0.0F, hud_tint);
    }
}
