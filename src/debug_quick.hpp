#pragma once

#include "color.hpp"
#include "common.hpp"

#include <raylib.h>

fwd_decl(DBGWidgetCallback);

struct ColorEditState {
    Color color;           // Local copy for widget display
    ColorHSV hsv_color;    // HSV representation
    Color *color_ptr;      // Pointer to actual color being edited
    BOOL updating;         // Flag to prevent circular callback updates
};

ColorEditState dbg_quick_get_color_edit_state(Color *color);

void dbg_quick_color_edit(C8 const *label,
                          Color *color,
                          ColorEditState *state,
                          DBGWidgetCallback on_click_cb);
