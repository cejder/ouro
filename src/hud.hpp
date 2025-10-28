#pragma once

#include "common.hpp"

#define HUD_TEXT_MAX_CHAR_COUNT 128

fwd_decl(String);

struct HUD {
    F32 top_height_perc;
    F32 bottom_height_perc;
    F32 left_width_perc;
    F32 right_width_perc;
};

HUD extern g_hud;

void hud_init(F32 top_perc, F32 bottom_perc, F32 left_perc, F32 right_perc);
void hud_update(F32 dt, F32 dtu);
void hud_draw_2d_hud();
void hud_draw_2d_hud_sketch();
