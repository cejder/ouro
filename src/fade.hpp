#pragma once

#include "common.hpp"
#include "ease.hpp"

#include <raylib.h>

fwd_decl(ATexture);

#define SCREEN_FADE_DEFAULT_DURATION 0.5F
#define SCREEN_FADE_QUICK_DEFAULT_DURATION 0.25F

enum ScreenFadeType : U8 {
    SCREEN_FADE_TYPE_FADE_IN,
    SCREEN_FADE_TYPE_FADE_OUT,

    SCREEN_FADE_TYPE_WIPE_LEFT_IN,
    SCREEN_FADE_TYPE_WIPE_LEFT_OUT,

    SCREEN_FADE_TYPE_WIPE_RIGHT_IN,
    SCREEN_FADE_TYPE_WIPE_RIGHT_OUT,

    SCREEN_FADE_TYPE_WIPE_UP_IN,
    SCREEN_FADE_TYPE_WIPE_UP_OUT,

    SCREEN_FADE_TYPE_WIPE_DOWN_IN,
    SCREEN_FADE_TYPE_WIPE_DOWN_OUT,

    SCREEN_FADE_TYPE_COUNT,
};

struct ScreenFade {
    ScreenFadeType type;
    BOOL finished;
    F32 duration;
    F32 elapsed;
    Color color;
    EaseType ease_type;
    ATexture *texture;
};

ScreenFade extern g_screen_fade;

void screen_fade_init(ScreenFadeType type, F32 duration, Color color, EaseType ease_type, ATexture *texture);
BOOL screen_fade_update(F32 dt);
void screen_fade_draw_2d_hud();
F32 screen_fade_get_elapsed_perc();
F32 screen_fade_get_remaining_perc();
C8 const *screen_fade_type_to_cstr(ScreenFadeType type);
