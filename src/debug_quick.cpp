#include "debug_quick.hpp"
#include "asset.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "string.hpp"

ColorEditState dbg_quick_get_color_edit_state(Color *color) {
    ColorEditState state = {};
    state.color = *color;
    state.color_ptr = color;
    color_to_hsv(*color, &state.hsv_color);
    return state;
}

// Callback for when H slider changes
void static i_hsv_h_changed(void *data) {
    auto *state = (ColorEditState *)data;

    if (state->updating) { return; };

    state->updating = true;

    // Convert current HSV to RGB
    ColorHSV const hsv = {.h = state->hsv_color.h, .s = state->hsv_color.s, .v = state->hsv_color.v};
    Color rgb          = color_from_hsv(hsv);

    // Preserve the existing alpha value
    rgb.a = state->color.a;

    // Update the internal color representation
    state->color = rgb;

    // Update the actual color pointer (only if it's different from our internal color)
    if (state->color_ptr && state->color_ptr != &state->color) {
        *state->color_ptr = rgb;
    }

    state->updating = false;
}

// Callback for when S slider changes
void static i_hsv_s_changed(void *data) {
    auto const *state = (ColorEditState *)data;

    if (state->updating) { return; }
    i_hsv_h_changed(data);  // Reuse the same function as they do the same thing
}

// Callback for when V slider changes
void static i_hsv_v_changed(void *data) {
    auto const *state = (ColorEditState *)data;

    if (state->updating) { return; }
    i_hsv_h_changed(data);  // Reuse the same function as they do the same thing
}

// Callback for when RGB values change - update HSV
void static i_rgb_changed(void *data) {
    auto *state = (ColorEditState *)data;

    if (state->updating) { return; }

    state->updating = true;

    // The color has been updated by the slider, sync to HSV
    ColorHSV hsv;
    color_to_hsv(state->color, &hsv);

    // Update HSV values
    state->hsv_color.h = hsv.h;
    state->hsv_color.s = hsv.s;
    state->hsv_color.v = hsv.v;

    // Update the actual color pointer (only if it's different from our internal color)
    if (state->color_ptr && state->color_ptr != &state->color) {
        *state->color_ptr = state->color;
    }

    state->updating = false;
}

void static i_color_picker_changed(void *data) {
    auto *state = (ColorEditState *)data;

    if (state->updating) { return; }

    state->updating = true;

    // The color has been modified by the color picker widget
    // Update the actual color pointer (only if it's different from our internal color)
    if (state->color_ptr && state->color_ptr != &state->color) {
        *state->color_ptr = state->color;
    }

    // Update HSV values
    color_to_hsv(state->color, &state->hsv_color);

    state->updating = false;
}

void dbg_quick_color_edit(C8 const *label,
                          Color *color,
                          ColorEditState *state,
                          DBGWidgetCallback on_click_cb) {
    AFont *font                   = asset_get_font(c_debug__medium_font.cstr, c_debug__medium_font_size);
    Vector2 const color_view_size = {DBG_WINDOW_MIN_WIDTH, 64.0F};
    Vector2 const fb_size         = DBG_REF_FILLBAR_SIZE;

    // Update the persistent state from the actual color
    state->color     = *color;
    state->color_ptr = color;

    // Convert current RGB to HSV if not already updating
    if (!state->updating) { color_to_hsv(state->color, &state->hsv_color); }

    // Create the callback for the color view
    DBGWidgetCallback const color_view_cb = {i_color_picker_changed, state};

    // Use the persistent color with the color view widget
    if (label) { dwicv(label, &state->color, color_view_size, color_view_cb); }

    // RGB sliders with callback to update HSV
    Color const color_r_bar          = {255, 0, 0, state->color.r};
    DBGWidgetCallback const rgb_cb_r = {i_rgb_changed, state};
    dwifb(TS("R %d", state->color.r)->c, font, color_r_bar, DBG_FILLBAR_BG_COLOR, WHITE,
          fb_size, (F32)state->color.r, 0.0F, 255.0F, true, &state->color.r, DBG_WIDGET_DATA_TYPE_U8, rgb_cb_r);

    Color const color_g_bar          = {0, 255, 0, state->color.g};
    DBGWidgetCallback const rgb_cb_g = {i_rgb_changed, state};
    dwifb(TS("G %d", state->color.g)->c, font, color_g_bar, DBG_FILLBAR_BG_COLOR, WHITE,
          fb_size, (F32)state->color.g, 0.0F, 255.0F, true, &state->color.g, DBG_WIDGET_DATA_TYPE_U8, rgb_cb_g);

    Color const color_b_bar          = {0, 0, 255, state->color.b};
    DBGWidgetCallback const rgb_cb_b = {i_rgb_changed, state};
    dwifb(TS("B %d", state->color.b)->c, font, color_b_bar, DBG_FILLBAR_BG_COLOR, WHITE,
          fb_size, (F32)state->color.b, 0.0F, 255.0F, true, &state->color.b, DBG_WIDGET_DATA_TYPE_U8, rgb_cb_b);

    Color const color_a_bar = {255, 255, 255, state->color.a};
    dwifb(TS("A %d", state->color.a)->c, font, color_a_bar, DBG_FILLBAR_BG_COLOR, WHITE,
          fb_size, (F32)state->color.a, 0.0F, 255.0F, true, &state->color.a, DBG_WIDGET_DATA_TYPE_U8, DBG_WIDGET_CB_NONE);

    dwis(2.0F);

    // HSV sliders
    // Create a gradient color for the hue slider
    Color const hue_color        = color_from_hsv({state->hsv_color.h, 1.0F, 1.0F, 1.0F});
    DBGWidgetCallback const h_cb = {i_hsv_h_changed, state};
    dwifb(TS("H %.0f", state->hsv_color.h)->c, font, hue_color, DBG_FILLBAR_BG_COLOR, WHITE,
          fb_size, state->hsv_color.h, 0.0F, 360.0F, true, &state->hsv_color.h, DBG_WIDGET_DATA_TYPE_FLOAT, h_cb);

    // Saturation slider
    Color const sat_color        = color_from_hsv({state->hsv_color.h, state->hsv_color.s, 1.0F, 1.0F});
    DBGWidgetCallback const s_cb = {i_hsv_s_changed, state};
    dwifb(TS("S %.2f", state->hsv_color.s)->c, font, sat_color, DBG_FILLBAR_BG_COLOR, WHITE,
          fb_size, state->hsv_color.s, 0.0F, 1.0F, true, &state->hsv_color.s, DBG_WIDGET_DATA_TYPE_FLOAT, s_cb);

    // Value slider
    Color const val_color        = color_from_hsv({state->hsv_color.h, state->hsv_color.s, state->hsv_color.v, 1.0F});
    DBGWidgetCallback const v_cb = {i_hsv_v_changed, state};
    dwifb(TS("V %.2f", state->hsv_color.v)->c, font, val_color, DBG_FILLBAR_BG_COLOR, WHITE,
          fb_size, state->hsv_color.v, 0.0F, 1.0F, true, &state->hsv_color.v, DBG_WIDGET_DATA_TYPE_FLOAT, v_cb);

    if (on_click_cb.func) { dwib("Reset Color", font, NAYBEIGE, DBG_REF_BUTTON_SIZE, on_click_cb.func, on_click_cb.data); }
}
