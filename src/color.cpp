#include "color.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "math.hpp"
#include "std.hpp"

struct IBlinking {
    F32 time;
    BOOL forward;
    F32 duration;
};

IBlinking static i_blinking_very_very_fast = {
    .time     = 0.0F,
    .forward  = true,
    .duration = 0.125F,
};

IBlinking static i_blinking_very_fast = {
    .time     = 0.0F,
    .forward  = true,
    .duration = 0.25F,
};

IBlinking static i_blinking_fast = {
    .time     = 0.0F,
    .forward  = true,
    .duration = 0.5F,
};

IBlinking static i_blinking_kinda_fast = {
    .time     = 0.0F,
    .forward  = true,
    .duration = 0.75F,
};

IBlinking static i_blinking_regular = {
    .time     = 0.0F,
    .forward  = true,
    .duration = 1.0F,
};

IBlinking static i_blinking_slow = {
    .time     = 0.0F,
    .forward  = true,
    .duration = 2.0F,
};

IBlinking static i_blinking_very_slow = {
    .time     = 0.0F,
    .forward  = true,
    .duration = 4.0F,
};

IBlinking static i_blinking_very_very_slow = {
    .time     = 0.0F,
    .forward  = true,
    .duration = 8.0F,
};

void static i_update_blinking(IBlinking *blinking, F32 dtu) {
    if (blinking->forward) {
        blinking->time += dtu / blinking->duration;
    } else {
        blinking->time -= dtu / blinking->duration;
    }
    if (blinking->time >= blinking->duration) {
        blinking->time    = blinking->duration;
        blinking->forward = false;
    } else if (blinking->time <= 0.0F) {
        blinking->time    = 0.0F;
        blinking->forward = true;
    }
}

void color_update(F32 dtu) {
    i_update_blinking(&i_blinking_very_very_fast, dtu);
    i_update_blinking(&i_blinking_very_fast, dtu);
    i_update_blinking(&i_blinking_fast, dtu);
    i_update_blinking(&i_blinking_kinda_fast, dtu);
    i_update_blinking(&i_blinking_regular, dtu);
    i_update_blinking(&i_blinking_slow, dtu);
    i_update_blinking(&i_blinking_very_slow, dtu);
}

Color color_lerp(Color c1, Color c2, F32 t) {
    return ColorLerp(c1, c2, t);
}

Color color_random() {
    Color result = {};
    result.r = random_u8(0, 255);
    result.g = random_u8(0, 255);
    result.b = random_u8(0, 255);
    result.a = 255;
    return result;
}

Color color_variation(Color color, S32 variation) {
    Color result;

    F32 temp = (F32)color.r + (F32)random_s32(-variation, variation);
    if (temp < 0.0F) {
        temp = 0.0F;
    } else if (temp > 255.0F) {
        temp = 255.0F;
    }
    result.r = (U8)temp;

    temp = (F32)color.g + (F32)random_s32(-variation, variation);
    if (temp < 0.0F) {
        temp = 0.0F;
    } else if (temp > 255.0F) {
        temp = 255.0F;
    }
    result.g = (U8)temp;

    temp = (F32)color.b + (F32)random_s32(-variation, variation);
    if (temp < 0.0F) {
        temp = 0.0F;
    } else if (temp > 255.0F) {
        temp = 255.0F;
    }
    result.b = (U8)temp;

    result.a = color.a;
    return result;
}

F32 color_get_variance(Color c1, Color c2) {
    F32 const r = (F32)c1.r - (F32)c2.r;
    F32 const g = (F32)c1.g - (F32)c2.g;
    F32 const b = (F32)c1.b - (F32)c2.b;
    return math_sqrt_f32((r * r) + (g * g) + (b * b)) / (255.0F * math_sqrt_f32(3.0F));
}

Color color_saturated(Color color) {
    // Convert RGB to HSV
    F32 r = (F32)color.r / 255.0F;
    F32 g = (F32)color.g / 255.0F;
    F32 b = (F32)color.b / 255.0F;

    F32 const max_val = glm::max(r, glm::max(g, b));
    F32 const min_val = glm::min(r, glm::min(g, b));
    F32 const delta   = max_val - min_val;

    F32 h = 0.0F;
    F32 s = 0.0F;
    F32 v = max_val;

    if (delta != 0.0F) {
        s = delta / max_val;

        if (r == max_val) {
            h = (g - b) / delta;
        } else if (g == max_val) {
            h = 2.0F + ((b - r) / delta);
        } else {
            h = 4.0F + ((r - g) / delta);
        }

        h *= 60.0F;
        if (h < 0.0F) { h += 360.0F; }
    }

    // Increase saturation and brightness
    s = glm::min(s * 1.25F, 1.0F);  // Increase saturation by 25%, cap at 100%
    v = glm::min(v * 1.25F, 1.0F);  // Increase brightness by 25%, cap at 100%

    // Convert HSV back to RGB
    S32 const i = (S32)(h / 60.0F) % 6;
    F32 const f = (h / 60.0F) - (F32)i;
    F32 const p = v * (1.0F - s);
    F32 const q = v * (1.0F - f * s);
    F32 const t = v * (1.0F - (1.0F - f) * s);

    switch (i) {
        case 0: {
            r = v;
            g = t;
            b = p;
        } break;
        case 1: {
            r = q;
            g = v;
            b = p;
        } break;
        case 2: {
            r = p;
            g = v;
            b = t;
        } break;
        case 3: {
            r = p;
            g = q;
            b = v;
        } break;
        case 4: {
            r = t;
            g = p;
            b = v;
        } break;
        case 5: {
            r = v;
            g = p;
            b = q;
        } break;
        default: {
            _unreachable_();
        }
    }

    // Convert RGB from F32 back to U8 and return the result
    Color result;
    result.r = (U8)(r * 255.0F);
    result.g = (U8)(g * 255.0F);
    result.b = (U8)(b * 255.0F);
    result.a = color.a;

    return result;
}

Color color_random_vibrant() {
    Color result;
    S32 const max_component = 255;
    S32 const mid_component = random_u8(128, 255);
    S32 const min_component = random_u8(0, 127);

    U8 const rand_order = random_u8(0, 5);
    switch (rand_order) {
        case 0: {
            result.r = (U8)max_component;
            result.g = (U8)mid_component;
            result.b = (U8)min_component;
        } break;
        case 1: {
            result.r = (U8)max_component;
            result.b = (U8)mid_component;
            result.g = (U8)min_component;
        } break;
        case 2: {
            result.g = (U8)max_component;
            result.r = (U8)mid_component;
            result.b = (U8)min_component;
        } break;
        case 3: {
            result.g = (U8)max_component;
            result.b = (U8)mid_component;
            result.r = (U8)min_component;
        } break;
        case 4: {
            result.b = (U8)max_component;
            result.r = (U8)mid_component;
            result.g = (U8)min_component;
        } break;
        case 5: {
            result.b = (U8)max_component;
            result.g = (U8)mid_component;
            result.r = (U8)min_component;
        } break;
        default: {
            _unreachable_();
        }
    }
    result.a = 255;
    return result;
}

Color color_random_vibrant_locked(BOOL lock_r, U8 fixed_r, BOOL lock_g, U8 fixed_g, BOOL lock_b, U8 fixed_b) {
    Color result;

    // If all channels are locked, return them directly
    if (lock_r && lock_g && lock_b) {
        result.r = fixed_r;
        result.g = fixed_g;
        result.b = fixed_b;
        result.a = 255;
        return result;
    }

    // If exactly two channels are locked, randomize the third
    if (lock_r && lock_g) {
        result.r = fixed_r;
        result.g = fixed_g;
        result.b = lock_b ? fixed_b : random_u8(0, 255);
        result.a = 255;
        return result;
    }
    if (lock_r && lock_b) {
        result.r = fixed_r;
        result.b = fixed_b;
        result.g = lock_g ? fixed_g : random_u8(0, 255);
        result.a = 255;
        return result;
    }
    if (lock_g && lock_b) {
        result.g = fixed_g;
        result.b = fixed_b;
        result.r = lock_r ? fixed_r : random_u8(0, 255);
        result.a = 255;
        return result;
    }

    // If only one channel is locked, ensure at least one max component
    if (lock_r) {
        result.r = fixed_r;
        if (fixed_r == 255) {
            // If locked at max, randomize the others freely
            result.g = random_u8(0, 255);
            result.b = random_u8(0, 255);
        } else {
            // Otherwise, make one of the other channels max
            if (random_u8(0, 1)) {
                result.g = 255;
                result.b = random_u8(0, 127);
            } else {
                result.b = 255;
                result.g = random_u8(0, 127);
            }
        }
        result.a = 255;
        return result;
    }
    if (lock_g) {
        result.g = fixed_g;
        if (fixed_g == 255) {
            result.r = random_u8(0, 255);
            result.b = random_u8(0, 255);
        } else {
            if (random_u8(0, 1)) {
                result.r = 255;
                result.b = random_u8(0, 127);
            } else {
                result.b = 255;
                result.r = random_u8(0, 127);
            }
        }
        result.a = 255;
        return result;
    }
    if (lock_b) {
        result.b = fixed_b;
        if (fixed_b == 255) {
            result.r = random_u8(0, 255);
            result.g = random_u8(0, 255);
        } else {
            if (random_u8(0, 1)) {
                result.r = 255;
                result.g = random_u8(0, 127);
            } else {
                result.g = 255;
                result.r = random_u8(0, 127);
            }
        }
        result.a = 255;
        return result;
    }

    // No locked channels? Use the original function
    return color_random_vibrant();
}

Color color_random_pastel() {
    Color result;
    U8 const base = 192;

    result.r = base + random_u8(0, 63);
    result.g = base + random_u8(0, 63);
    result.b = base + random_u8(0, 63);
    result.a = 255;
    return result;
}

Color color_random_dark() {
    Color result;
    U8 const base = 64;

    result.r = base + random_u8(0, 63);
    result.g = base + random_u8(0, 63);
    result.b = base + random_u8(0, 63);
    result.a = 255;
    return result;
}

Color color_random_with_same_contrast(Color color) {
    Color result;
    Color const original_contrast = color_contrast(color);

    for (;;) {
        result = color_random();
        Color const result_contrast = color_contrast(result);
        if (color_is_equal(original_contrast, result_contrast)) { break; }
    }

    return result;
}

Color color_sync_blinking_very_very_fast(Color c1, Color c2) {
    return color_ease(EASE_IN_OUT_QUAD, i_blinking_very_very_fast.time, c1, c2, i_blinking_very_very_fast.duration);
}

Color color_sync_blinking_very_fast(Color c1, Color c2) {
    return color_ease(EASE_IN_OUT_QUAD, i_blinking_very_fast.time, c1, c2, i_blinking_very_fast.duration);
}

Color color_sync_blinking_fast(Color c1, Color c2) {
    return color_ease(EASE_IN_OUT_QUAD, i_blinking_fast.time, c1, c2, i_blinking_fast.duration);
}

Color color_sync_blinking_kinda_fast(Color c1, Color c2) {
    return color_ease(EASE_IN_OUT_QUAD, i_blinking_kinda_fast.time, c1, c2, i_blinking_kinda_fast.duration);
}

Color color_sync_blinking_regular(Color c1, Color c2) {
    return color_ease(EASE_IN_OUT_QUAD, i_blinking_regular.time, c1, c2, i_blinking_regular.duration);
}

Color color_sync_blinking_slow(Color c1, Color c2) {
    return color_ease(EASE_IN_OUT_QUAD, i_blinking_slow.time, c1, c2, i_blinking_slow.duration);
}

Color color_sync_blinking_very_slow(Color c1, Color c2) {
    return color_ease(EASE_IN_OUT_QUAD, i_blinking_very_slow.time, c1, c2, i_blinking_very_slow.duration);
}

Color color_sync_blinking_very_very_slow(Color c1, Color c2) {
    return color_ease(EASE_IN_OUT_QUAD, i_blinking_very_very_slow.time, c1, c2, i_blinking_very_very_slow.duration);
}

Color color_ease_transparency(EaseType ease_type, Color color, F32 t, F32 d) {
    Color result = {};
    result.r = color.r;
    result.g = color.g;
    result.b = color.b;
    result.a = (U8)ease_use(ease_type, t, 0.0F, 255.0F, d);
    return result;
}

Color color_ease(EaseType ease_type, F32 t, Color c1, Color c2, F32 d) {
    Color result = {};
    result.r = (U8)ease_use(ease_type, t, (F32)c1.r, (F32)c2.r - (F32)c1.r, d);
    result.g = (U8)ease_use(ease_type, t, (F32)c1.g, (F32)c2.g - (F32)c1.g, d);
    result.b = (U8)ease_use(ease_type, t, (F32)c1.b, (F32)c2.b - (F32)c1.b, d);
    result.a = (U8)ease_use(ease_type, t, (F32)c1.a, (F32)c2.a - (F32)c1.a, d);
    return result;
}

Color color_invert(Color color) {
    Color result = {};
    result.r = 255 - color.r;
    result.g = 255 - color.g;
    result.b = 255 - color.b;
    result.a = color.a;
    return result;
}

Color color_darken(Color color, F32 amount) {
    Color result;

    if (amount >= 0.0F) {
        // Positive amount: darken
        amount = glm::min(amount, 1.0F);  // Clamp to max 1.0
        result.r = (U8)glm::clamp((F32)color.r * (1.0F - amount), 0.0F, 255.0F);
        result.g = (U8)glm::clamp((F32)color.g * (1.0F - amount), 0.0F, 255.0F);
        result.b = (U8)glm::clamp((F32)color.b * (1.0F - amount), 0.0F, 255.0F);
    } else {
        // Negative amount: lighten
        F32 const lighten_amount = glm::min(-amount, 1.0F);  // Make positive and clamp
        result.r = (U8)glm::clamp((F32)color.r + ((255.0F - (F32)color.r) * lighten_amount), 0.0F, 255.0F);
        result.g = (U8)glm::clamp((F32)color.g + ((255.0F - (F32)color.g) * lighten_amount), 0.0F, 255.0F);
        result.b = (U8)glm::clamp((F32)color.b + ((255.0F - (F32)color.b) * lighten_amount), 0.0F, 255.0F);
    }

    result.a = color.a;
    return result;
}

F32 color_luminance(Color color) {
    F32 const r = (F32)color.r / 255.0F;
    F32 const g = (F32)color.g / 255.0F;
    F32 const b = (F32)color.b / 255.0F;
    return (0.299F * r) + (0.587F * g) + (0.114F * b);
}

Color color_blend(Color c1, Color c2) {
    Color result;
    result.r = (c1.r + c2.r) / 2;
    result.g = (c1.g + c2.g) / 2;
    result.b = (c1.b + c2.b) / 2;
    result.a = 255;
    return result;
}

Color color_contrast(Color color) {
    F32 const lum = color_luminance(color) * 255.0F;
    if (lum > 128.0F) { return {0, 0, 0, color.a}; }
    return {255, 255, 255, color.a};
}

void color_to_vec4(Color color, F32 *vec4) {
    vec4[0] = (F32)color.r / 255.0F;
    vec4[1] = (F32)color.g / 255.0F;
    vec4[2] = (F32)color.b / 255.0F;
    vec4[3] = (F32)color.a / 255.0F;
}

void color_from_vec4(Color *color, F32 const *vec4) {
    color->r = (U8)(vec4[0] * 255.0F);
    color->g = (U8)(vec4[1] * 255.0F);
    color->b = (U8)(vec4[2] * 255.0F);
    color->a = (U8)(vec4[3] * 255.0F);
}

BOOL color_is_equal(Color c1, Color c2) {
    return c1.r == c2.r && c1.g == c2.g && c1.b == c2.b && c1.a == c2.a;
}

BOOL color_hsv_is_equal(ColorHSV c1, ColorHSV c2) {
    return c1.h == c2.h && c1.s == c2.s && c1.v == c2.v && c1.a == c2.a;
}

BOOL colorf_is_equal(ColorF c1, ColorF c2) {
    return F32_EQUAL(c1.r, c2.r) && F32_EQUAL(c1.g, c2.g) && F32_EQUAL(c1.b, c2.b) && F32_EQUAL(c1.a, c2.a);
}

F32 static i_component_difference(U8 a, U8 b) {
    return (F32)((a - b) * (a - b));
}

BOOL color_is_similar(Color c1, Color c2, F32 threshold_perc) {
    _assert_(threshold_perc >= 0.0F, "Threshold percentage must be greater than or equal to 0.0F");
    _assert_(threshold_perc <= 1.0F, "Threshold percentage must be less than or equal to 1.0F");

    F32 const max_distance  = math_sqrt_f32(3.0F * 255.0F * 255.0F);
    F32 const distance      = math_sqrt_f32(i_component_difference(c1.r, c2.r) + i_component_difference(c1.g, c2.g) + i_component_difference(c1.b, c2.b));
    F32 const norm_distance = distance / max_distance;

    return norm_distance <= threshold_perc;
}

Color color_from_cstr(C8 const *str) {
    _assertf_(str[0] == '#', "Color string must start with '#': %s", str);

    Color result = {};

    U32 hex = 0;
    ou_sscanf(str + 1, "%x", &hex);

    result.r = (hex >> 24) & 0xFF;
    result.g = (hex >> 16) & 0xFF;
    result.b = (hex >> 8) & 0xFF;
    result.a = hex & 0xFF;

    return result;
}

Color color_from_texture_dominant(ATexture *texture) {
    Image img = LoadImageFromTexture(texture->base);

    F64 lin_r = 0;
    F64 lin_g = 0;
    F64 lin_b = 0;
    F64 weight_sum = 0;

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            Color c = GetImageColor(img, x, y);

            // Convert to linear
            F32 r = (F32)(c.r) / 255.0F;
            F32 g = (F32)(c.g) / 255.0F;
            F32 b = (F32)(c.b) / 255.0F;

            r = (r <= 0.04045F) ? r / 12.92F : glm::pow((r + 0.055F) / 1.055F, 2.4F);
            g = (g <= 0.04045F) ? g / 12.92F : glm::pow((g + 0.055F) / 1.055F, 2.4F);
            b = (b <= 0.04045F) ? b / 12.92F : glm::pow((b + 0.055F) / 1.055F, 2.4F);

            // Weight by luminance (bright areas matter more for ambient light)
            F32 lum = (0.2126F * r) + (0.7152F * g) + (0.0722F * b);
            lum = glm::max(lum, 0.01F);  // avoid zero

            lin_r += r * lum;
            lin_g += g * lum;
            lin_b += b * lum;
            weight_sum += lum;
        }
    }

    if (weight_sum < 1e-6) {
        return (Color){100, 150, 200, 255}; // fallback
    }

    // Normalize
    F32 avg_r = (F32)(lin_r / weight_sum);
    F32 avg_g = (F32)(lin_g / weight_sum);
    F32 avg_b = (F32)(lin_b / weight_sum);

    // Back to sRGB
    auto to_srgb = [](F32 v) -> U8 {
        v = glm::clamp(v, 0.0F, 1.0F);
        if (v <= 0.0031308F) { return (U8)lroundf(v * 12.92F * 255.0F); };
        return (U8)glm::round((1.055F * powf(v, 1.0F / 2.4F) - 0.055F) * 255.0F);
    };

    UnloadImage(img);
    return { to_srgb(avg_r), to_srgb(avg_g), to_srgb(avg_b), 255 };
}

Color color_from_texture(ATexture *texture) {
    return color_from_texture_rl(&texture->base);
}

Color color_from_texture_rl(Texture *texture) {
    Image const img = LoadImageFromTexture(*texture);

    SZ total_red   = 0;
    SZ total_green = 0;
    SZ total_blue  = 0;

    for (S32 y = 0; y < img.height; ++y) {
        for (S32 x = 0; x < img.width; ++x) {
            Color const color = GetImageColor(img, x, y);
            total_red   += color.r;
            total_green += color.g;
            total_blue  += color.b;
        }
    }

    SZ const pixel_count = (SZ)img.width * (SZ)img.height;
    SZ const avg_red     = total_red / pixel_count;
    SZ const avg_green   = total_green / pixel_count;
    SZ const avg_blue    = total_blue / pixel_count;

    return {(U8)avg_red, (U8)avg_green, (U8)avg_blue, 255};
}

void color_hex_from_color(Color c, C8 *buf) {
    ou_sprintf(buf, "%02X%02X%02X%02X", (U8)(c.r), (U8)(c.g), (U8)(c.b), (U8)(c.a));
}

ColorF color_normalize(Color color) {
    ColorF result;
    result.r = (F32)color.r / 255.0F;
    result.g = (F32)color.g / 255.0F;
    result.b = (F32)color.b / 255.0F;
    result.a = (F32)color.a / 255.0F;
    return result;
}

void color_to_hsv(Color color, ColorHSV *color_hsv) {
    color_hsv->a = color.a;

    F32 const r = (F32)color.r / 255.0F;
    F32 const g = (F32)color.g / 255.0F;
    F32 const b = (F32)color.b / 255.0F;

    F32 const max_val = glm::max(r, glm::max(g, b));
    F32 const min_val = glm::min(r, glm::min(g, b));
    F32 const delta   = max_val - min_val;

    color_hsv->h = 0.0F;
    color_hsv->s = 0.0F;
    color_hsv->v = max_val;

    if (delta != 0.0F) {
        color_hsv->s = delta / max_val;

        if (r == max_val) {
            color_hsv->h = (g - b) / delta;
        } else if (g == max_val) {
            color_hsv->h = 2.0F + ((b - r) / delta);
        } else {
            color_hsv->h = 4.0F + ((r - g) / delta);
        }

        color_hsv->h *= 60.0F;
        if (color_hsv->h < 0.0F) { color_hsv->h += 360.0F; }
    }
}

Color color_from_hsv(ColorHSV color_hsv) {
    Color result = {};

    if (color_hsv.s <= 0.0F) {
        // Achromatic (grey)
        result.r = (U8)(color_hsv.v * 255.0F);
        result.g = (U8)(color_hsv.v * 255.0F);
        result.b = (U8)(color_hsv.v * 255.0F);
        result.a = 255;
        return result;
    }

    color_hsv.h = math_mod_f32(color_hsv.h, 360.0F);
    if (color_hsv.h < 0.0F) { color_hsv.h += 360.0F; }
    color_hsv.h /= 60.0F;  // sector 0 to 5

    S32 const i = (S32)color_hsv.h;
    F32 const f = color_hsv.h - (F32)i;  // factorial part of h

    F32 const p = color_hsv.v * (1.0F - color_hsv.s);
    F32 const q = color_hsv.v * (1.0F - color_hsv.s * f);
    F32 const t = color_hsv.v * (1.0F - color_hsv.s * (1.0F - f));

    F32 r = 0.0F;
    F32 g = 0.0F;
    F32 b = 0.0F;

    switch (i) {
        case 0: {
            r = color_hsv.v;
            g = t;
            b = p;
        } break;
        case 1: {
            r = q;
            g = color_hsv.v;
            b = p;
        } break;
        case 2: {
            r = p;
            g = color_hsv.v;
            b = t;
        } break;
        case 3: {
            r = p;
            g = q;
            b = color_hsv.v;
        } break;
        case 4: {
            r = t;
            g = p;
            b = color_hsv.v;
        } break;
        default: {
            r = color_hsv.v;
            g = p;
            b = q;
        } break;
    }

    result.r = (U8)(r * 255.0F);
    result.g = (U8)(g * 255.0F);
    result.b = (U8)(b * 255.0F);
    result.a = 255;

    return result;
}

void color_to_glm_vec4(Color color, glm::vec4 *color_vec_ptr) {
    color_vec_ptr->r = (F32)color.r / 255.0F;
    color_vec_ptr->g = (F32)color.g / 255.0F;
    color_vec_ptr->b = (F32)color.b / 255.0F;
    color_vec_ptr->a = (F32)color.a / 255.0F;
}

Color color_from_glm_vec3(glm::vec3 color_vec) {
    return {
        (U8)(color_vec.r / 255.0F),
        (U8)(color_vec.g / 255.0F),
        (U8)(color_vec.b / 255.0F),
        255,
    };
}

Color color_from_glm_vec4(glm::vec4 color_vec) {
    return {
        (U8)(color_vec.r / 255.0F),
        (U8)(color_vec.g / 255.0F),
        (U8)(color_vec.b / 255.0F),
        (U8)(color_vec.a / 255.0F),
    };
}

Color color_hue_by_seed(U32 seed) {
    F32 const hue = math_mod_f32((F32)seed * 360.0F / 20.0F, 360.0F);  // 20 colors = 18° apart
    return color_from_hsv({hue, 0.85F, 0.9F, 1.0F});
}

ColorF color_to_colorf(Color color) {
    return {(F32)color.r/255.0F, (F32)color.g/255.0F, (F32)color.b/255.0F, (F32)color.a/255.0F};
}
