#pragma once

#include "common.hpp"

#include <raylib.h>

fwd_decl(ATexture);
fwd_decl(AFont);

void scene_extra_draw_menu_art(ATexture *texture,
                               AFont *title_font,
                               C8 const *title,
                               Vector2 title_position,
                               Color title_color,
                               Color title_color_shadow,
                               Vector2 title_shadow_offset);
