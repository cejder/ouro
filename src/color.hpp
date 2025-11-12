#pragma once

#include "common.hpp"
#include "ease.hpp"

#include <raylib.h>
#include <glm/fwd.hpp>

// 8 chars for RRGGBBAA + 1 for null terminator
#define COLOR_HEX_SIZE 9

fwd_decl(ATexture);

struct ColorF {
    F32 r;
    F32 g;
    F32 b;
    F32 a;
};

struct ColorHSV {
    F32 h;  // Hue (0-360 degrees)
    F32 s;  // Saturation (0.0-1.0)
    F32 v;  // Value/Brightness (0.0-1.0)
    F32 a;  // Alpha (0.0-1.0)
};

void color_update(F32 dtu);
Color color_lerp(Color c1, Color c2, F32 t);
Color color_random();
Color color_variation(Color color, S32 variation);
F32 color_get_variance(Color c1, Color c2);
Color color_saturated(Color color);
Color color_random_vibrant();
Color color_random_vibrant_locked(BOOL lock_r, U8 fixed_r, BOOL lock_g, U8 fixed_g, BOOL lock_b, U8 fixed_b);
Color color_random_pastel();
Color color_random_dark();
Color color_random_with_same_contrast(Color color);
Color color_sync_blinking_very_very_fast(Color c1, Color c2);
Color color_sync_blinking_very_fast(Color c1, Color c2);
Color color_sync_blinking_fast(Color c1, Color c2);
Color color_sync_blinking_kinda_fast(Color c1, Color c2);
Color color_sync_blinking_regular(Color c1, Color c2);
Color color_sync_blinking_slow(Color c1, Color c2);
Color color_sync_blinking_very_slow(Color c1, Color c2);
Color color_sync_blinking_very_very_slow(Color c1, Color c2);
Color color_ease_transparency(EaseType ease_type, Color color, F32 t, F32 d);
Color color_ease(EaseType ease_type, F32 t, Color c1, Color c2, F32 d);
Color color_invert(Color color);
Color color_darken(Color color, F32 amount);
F32 color_luminance(Color color);
Color color_blend(Color c1, Color c2);
Color color_contrast(Color color);
void color_to_vec4(Color color, F32 *vec4);
void color_from_vec4(Color *color, F32 const *vec4);
BOOL color_is_equal(Color c1, Color c2);
BOOL color_hsv_is_equal(ColorHSV c1, ColorHSV c2);
BOOL colorf_is_equal(ColorF c1, ColorF c2);
BOOL color_is_similar(Color c1, Color c2, F32 threshold_perc);
Color color_from_cstr(C8 const *str);
Color color_from_texture_dominant(ATexture *texture);
Color color_from_texture(ATexture *texture);
Color color_from_texture_rl(Texture *texture);
void color_hex_from_color(Color c, C8 *buf);
ColorF color_normalize(Color color);
void color_to_hsv(Color color, ColorHSV *color_hsv);
Color color_from_hsv(ColorHSV color_hsv);
void color_to_glm_vec4(Color color, glm::vec4 *color_vec_ptr);
Color color_from_glm_vec3(glm::vec3 color_vec);
Color color_from_glm_vec4(glm::vec4 color_vec);
Color color_hue_by_seed(U32 seed);
ColorF color_to_colorf(Color color);
