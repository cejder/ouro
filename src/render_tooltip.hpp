#pragma once

#include "common.hpp"

#include <raylib.h>

fwd_decl(String);
fwd_decl(AFont);

#define RENDER_TOOLTIP_MAX_ROWS 64

struct RenderTooltipRow {
    String *key;
    String *value;
};

struct RenderTooltip {
    AFont *font;
    RenderTooltipRow rows[RENDER_TOOLTIP_MAX_ROWS];
    SZ row_count;
    F32 widest_key;
    SZ widest_key_char_count;
    F32 widest_value;
    SZ widest_value_char_count;
    String *spacer;
    F32 spacer_width;
    Vector3 position;
    BOOL is_world_position;
};

void render_tooltip_init(AFont *font, RenderTooltip *tt, String *spacer, Vector3 position, BOOL is_world_position);
void render_tooltip_add(RenderTooltip *tt, RenderTooltipRow row);
void render_tooltip_add_progress(RenderTooltip *tt, String *key, F32 current, F32 total);
void render_tooltip_draw(RenderTooltip *tt);

#define rib_STR(tt, key_str, value_str)            render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#ffffffa0}%s", value_str)})
#define rib_S32(tt, key_str, value)                render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#ffd700ff}%d", value)})
#define rib_U32(tt, key_str, value)                render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#ffd700ff}%u", value)})
#define rib_U64(tt, key_str, value)                render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#ffd700ff}%" PRIu64, value)})
#define rib_SZ(tt, key_str, value)                 render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#ffd700ff}%zu", value)})
#define rib_F32(tt, key_str, value)                render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#ffd700ff}%.2f", value)})
#define rib_MS(tt, key_str, value)                 render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#ffd700ff}%.2f ms", value)})
#define rib_PERC(tt, key_str, value)               render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#ffd700ff}%.2f %%", value)})
#define rib_BOOL(tt, key_str, value)               render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#%sff}%s", (value) ? "55ff55" : "ff5555", (value) ? "TRUE" : "FALSE")})
#define rib_V3(tt, key_str, vec)                   render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#ff5555ff}%6.2f \\ouc{#55ff55ff}%6.2f \\ouc{#5555ffff}%6.2f", (vec).x, (vec).y, (vec).z)})
#define rib_V2(tt, key_str, vec)                   render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#ff5555ff}%6.2f \\ouc{#55ff55ff}%6.2f", (vec).x, (vec).y)})
#define rib_MODEL(tt, key_str, name, vertex_count) render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#ffffffa0}%s \\ouc{#ffd700ff}(%zu)", name, vertex_count)})
#define rib_COLOR(tt, key_str, color_hex, color)   render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#%s}%u %u %u %u (#%s)", color_hex, (color).r, (color).g, (color).b, (color).a, color_hex)})
#define rib_HEALTH(tt, key_str, cur, max)          render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#ff0066ff}%u/%u", cur, max)})
#define rib_ENTITY(tt, key_str, world, id)         render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#ffffffa0}%u (%s, GEN %u, %s)", id, (world)->name[id], (world)->generation[id], entity_type_to_cstr((world)->type[id]))})
#define rib_TEXT(tt, key_str, value_str)           render_tooltip_add(tt, {TS(key_str), TS("\\ouc{#ffffffa0}%s", value_str)})
#define rib_PROGRESS(tt, key_str, current, total)  render_tooltip_add_progress(tt, TS(key_str), current, total)
