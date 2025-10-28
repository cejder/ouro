#include "fade.hpp"

#include "assert.hpp"
#include "asset.hpp"
#include "cvar.hpp"
#include "render.hpp"

#define FADE_CHECKERBOARD_SIZE 64

ScreenFade g_screen_fade = {};

C8 static const *i_screen_fade_type_to_cstr[SCREEN_FADE_TYPE_COUNT] = {
    "FADE_IN",       "FADE_OUT",
    "WIPE_LEFT_IN",  "WIPE_LEFT_OUT",
    "WIPE_RIGHT_IN", "WIPE_RIGHT_OUT",
    "WIPE_UP_IN",    "WIPE_UP_OUT",
    "WIPE_DOWN_IN",  "WIPE_DOWN_OUT",
};

void screen_fade_init(ScreenFadeType type, F32 duration, Color color, EaseType ease_type, ATexture *texture) {
    g_screen_fade.type      = type;
    g_screen_fade.finished  = false;
    g_screen_fade.duration  = duration;
    g_screen_fade.elapsed   = 0.0F;
    g_screen_fade.color     = color;
    g_screen_fade.ease_type = ease_type;
    g_screen_fade.texture   = texture;
}

BOOL screen_fade_update(F32 dt) {
    if (g_screen_fade.finished) { return true; }

    g_screen_fade.elapsed += dt;
    if (g_screen_fade.elapsed >= g_screen_fade.duration) {
        g_screen_fade.elapsed  = g_screen_fade.duration;
        g_screen_fade.finished = true;
    }

    return g_screen_fade.finished;
}

void screen_fade_draw_2d_hud() {
    if (g_screen_fade.finished) { return; }

    F32 alpha              = 0.0F;
    F32 const perc_elapsed = g_screen_fade.elapsed / g_screen_fade.duration;

    switch (g_screen_fade.type) {
        case SCREEN_FADE_TYPE_FADE_IN: {
            alpha = ease_use(g_screen_fade.ease_type, perc_elapsed, 0.0F, 1.0F, 1.0F);
        } break;
        case SCREEN_FADE_TYPE_FADE_OUT: {
            alpha = ease_use(g_screen_fade.ease_type, 1.0F - perc_elapsed, 0.0F, 1.0F, 1.0F);
        } break;

        case SCREEN_FADE_TYPE_WIPE_LEFT_IN:
        case SCREEN_FADE_TYPE_WIPE_LEFT_OUT:
        case SCREEN_FADE_TYPE_WIPE_RIGHT_IN:
        case SCREEN_FADE_TYPE_WIPE_RIGHT_OUT:
        case SCREEN_FADE_TYPE_WIPE_UP_IN:
        case SCREEN_FADE_TYPE_WIPE_UP_OUT:
        case SCREEN_FADE_TYPE_WIPE_DOWN_IN:
        case SCREEN_FADE_TYPE_WIPE_DOWN_OUT: {
            // These fade types don't use traditional alpha blending - they use geometric shapes instead.
        } break;

        default: {
            _unreachable_();
        }
    }

    Vector2 const res  = {(F32)c_video__render_resolution_width, (F32)c_video__render_resolution_height};
    F32 const progress = ease_use(g_screen_fade.ease_type, perc_elapsed, 0.0F, 1.0F, 1.0F);

    switch (g_screen_fade.type) {
        case SCREEN_FADE_TYPE_WIPE_LEFT_IN: {
            F32 const wipe_width = res.x * progress;
            d2d_rectangle_rec({0.0F, 0.0F, wipe_width, res.y}, g_screen_fade.color);
            return;
        }
        case SCREEN_FADE_TYPE_WIPE_LEFT_OUT: {
            F32 const start_x    = res.x * progress;
            F32 const wipe_width = res.x - start_x + 1.0F;
            d2d_rectangle_rec({start_x, 0.0F, wipe_width, res.y}, g_screen_fade.color);
            return;
        }

        case SCREEN_FADE_TYPE_WIPE_RIGHT_IN: {
            F32 const wipe_width = res.x * progress;
            F32 const start_x    = res.x - wipe_width;
            d2d_rectangle_rec({start_x, 0.0F, wipe_width + 1.0F, res.y}, g_screen_fade.color);
            return;
        }
        case SCREEN_FADE_TYPE_WIPE_RIGHT_OUT: {
            F32 const wipe_width = res.x * (1.0F - progress);
            d2d_rectangle_rec({0.0F, 0.0F, wipe_width, res.y}, g_screen_fade.color);
            return;
        }

        case SCREEN_FADE_TYPE_WIPE_UP_IN: {
            F32 const wipe_height = res.y * progress;
            d2d_rectangle_rec({0.0F, 0.0F, res.x, wipe_height}, g_screen_fade.color);
            return;
        }
        case SCREEN_FADE_TYPE_WIPE_UP_OUT: {
            F32 const start_y     = res.y * progress;
            F32 const wipe_height = res.y - start_y + 1.0F;
            d2d_rectangle_rec({0.0F, start_y, res.x, wipe_height}, g_screen_fade.color);
            return;
        }

        case SCREEN_FADE_TYPE_WIPE_DOWN_IN: {
            F32 const wipe_height = res.y * progress;
            F32 const start_y     = res.y - wipe_height;
            d2d_rectangle_rec({0.0F, start_y, res.x, wipe_height + 1.0F}, g_screen_fade.color);
            return;
        }
        case SCREEN_FADE_TYPE_WIPE_DOWN_OUT: {
            F32 const wipe_height = res.y * (1.0F - progress);
            d2d_rectangle_rec({0.0F, 0.0F, res.x, wipe_height}, g_screen_fade.color);
            return;
        }

        default: {
            break;
        }
    }

    if (g_screen_fade.texture) {
        Rectangle const src = {0.0F, 0.0F, (F32)g_screen_fade.texture->base.width, (F32)g_screen_fade.texture->base.height};
        Rectangle const dst = {0.0F, 0.0F, res.x, res.y};
        d2d_texture_pro(g_screen_fade.texture, src, dst, {0.0F, 0.0F}, 0.0F, Fade(g_screen_fade.color, alpha));
    } else {
        d2d_rectangle_rec({0.0F, 0.0F, res.x, res.y}, Fade(g_screen_fade.color, alpha));
    }
}

F32 screen_fade_get_elapsed_perc() {
    return g_screen_fade.elapsed / g_screen_fade.duration;
}

F32 screen_fade_get_remaining_perc() {
    return 1.0F - (g_screen_fade.elapsed / g_screen_fade.duration);
}

C8 const *screen_fade_type_to_cstr(ScreenFadeType type) {
    return i_screen_fade_type_to_cstr[type];
}
