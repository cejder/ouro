#include "render_tooltip.hpp"
#include "asset.hpp"
#include "log.hpp"
#include "math.hpp"
#include "render.hpp"
#include "std.hpp"
#include "string.hpp"

void render_tooltip_init(AFont *font, RenderTooltip *tt, String *spacer, Vector3 position, BOOL is_world_position) {
    tt->font              = font;
    tt->row_count         = 0;
    tt->spacer            = spacer;
    tt->spacer_width      = measure_text(font, spacer->c).x;
    tt->position          = position;
    tt->is_world_position = is_world_position;
}

void render_tooltip_add(RenderTooltip *tt, RenderTooltipRow row) {
    if (tt->row_count >= RENDER_TOOLTIP_MAX_ROWS) {
        llw("Too many rows in render tooltip (%d MAX)", RENDER_TOOLTIP_MAX_ROWS);
        return;
    }

    tt->rows[tt->row_count++] = row;
    tt->widest_key              = glm::max(measure_text_ouc(tt->font, row.key->c).x, tt->widest_key);
    tt->widest_value            = glm::max(measure_text_ouc(tt->font, row.value->c).x, tt->widest_value);
    tt->widest_key_char_count   = glm::max(ou_strlen(row.key->c), tt->widest_key_char_count);
    tt->widest_value_char_count = glm::max(ou_strlen(row.value->c), tt->widest_value_char_count);
}

void render_tooltip_add_progress(RenderTooltip *tt, String *key, F32 current, F32 total) {
    if (total <= 0.0F) {
        render_tooltip_add(tt, {key, TS("\\ouc{#ffffffa0}N/A")});
        return;
    }

    F32 const progress      = glm::clamp(current / total, 0.0F, 1.0F);
    S32 const bar_length    = 15;
    S32 const filled_length = (S32)(progress * (F32)bar_length);

    String *filled_part = string_create_empty(MEMORY_TYPE_TARENA);
    String *empty_part  = string_create_empty(MEMORY_TYPE_TARENA);

    for (S32 i = 0; i < filled_length; ++i)                { string_append(filled_part, "O"); }
    for (S32 i = 0; i < (bar_length - filled_length); ++i) { string_append(empty_part,  " "); }

    String *bar = TS("[\\ouc{#00ff00ff}%s\\ouc{#333333ff}%s\\ouc{#ffffffff}] %.1f/%.1fs (%.0f%%)", filled_part->c, empty_part->c, current, total, progress * 100.0F);

    render_tooltip_add(tt, {key, bar});
}

void render_tooltip_draw(RenderTooltip *tt) {
    Vector2 const render_res = render_get_render_resolution();

    String *out = string_create_empty(MEMORY_TYPE_TARENA);

    for (SZ i = 0; i < tt->row_count; ++i) {
        string_appendf(out, "%-*s%s%s\n", (S32)tt->widest_key_char_count, tt->rows[i].key->c, tt->spacer->c, tt->rows[i].value->c);
    }
    string_trim_back(out, "\n");

    Vector2 base_position = {tt->position.x, tt->position.y};
    if (tt->is_world_position) {
        base_position = GetWorldToScreenEx(tt->position, c3d_get(), (S32)render_res.x, (S32)render_res.y);
    }

    Vector2 const content_size = {
        tt->widest_key + tt->spacer_width + tt->widest_value,
        (F32)((S32)tt->row_count * tt->font->font_size),
    };

    F32 const pad              = (F32)tt->font->font_size / 2.0F;
    Vector2 const tooltip_size = {content_size.x + (2.0F * pad), content_size.y + (2.0F * pad)};
    F32 const offset           = 15.0F;
    F32 const margin           = 10.0F;
    Vector2 final_position     = {base_position.x + offset, base_position.y + offset};

    if (final_position.x + tooltip_size.x > render_res.x - margin) {
        F32 const overhang = (final_position.x + tooltip_size.x) - (render_res.x - margin);
        final_position.x -= overhang;
    }

    if (final_position.y + tooltip_size.y > render_res.y - margin) {
        F32 const overhang = (final_position.y + tooltip_size.y) - (render_res.y - margin);
        final_position.y -= overhang;
    }

    final_position.x = glm::max(final_position.x, margin);
    final_position.y = glm::max(final_position.y, margin);

    Vector2 const screen_position = {final_position.x + pad, final_position.y + pad};
    F32 const roundness           = 0.05F;
    S32 const segments            = 16;
    Color const bg_color          = Fade(NEARBLACK, 0.75F);
    Color const left_bg_color     = Fade(BLACK, 0.85F);

    Rectangle const bg = {
        final_position.x,
        final_position.y,
        tooltip_size.x,
        tooltip_size.y,
    };

    Rectangle const left_bg = {
        final_position.x,
        final_position.y,
        tt->widest_key + (tt->spacer_width / 2.0F) + pad,
        tooltip_size.y,
    };

    DrawRectangleRounded(bg, roundness, segments, bg_color);
    DrawRectangleRounded(left_bg, roundness, segments, left_bg_color);

    d2d_text_ouc_shadow(tt->font, out->c, screen_position, WHITE, BLACK, {1.0F, 1.0F});
}
