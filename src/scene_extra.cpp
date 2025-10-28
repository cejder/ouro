#include "scene_extra.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "message.hpp"
#include "profiler.hpp"
#include "render.hpp"
#include "time.hpp"

#include <glm/gtc/type_ptr.hpp>

void scene_extra_draw_menu_art(ATexture *texture,
                               AFont *title_font,
                               C8 const *title,
                               Vector2 title_position,
                               Color title_color,
                               Color title_color_shadow,
                               Vector2 title_shadow_offset) {
    // NOTE: This function is written like this on purpose. I believe at least the "artwork" in the
    // main menu can be chaotic or inefficient. Who cares. Maybe
    // something is misaglined or not centered. I do not care. It looks good enough for now.

    Vector2 const res              = render_get_render_resolution();;
    Color blinking_color           = color_sync_blinking_slow(Fade(WHITE, 1.0F), Fade(BEIGE, 0.75F));
    Rectangle src                  = {0.0F, 0.0F, (F32)texture->base.width, (F32)texture->base.height};
    F32 const texture_aspect_ratio = src.width / src.height;
    F32 const texture_width        = res.x / 2.0F;
    F32 texture_height             = texture_width / texture_aspect_ratio;
    F32 const half_screen_width    = res.x / 2.0F;

    texture_height = texture_width * (texture_height / texture_width);
    Rectangle dst  = {(half_screen_width / 2.0F) - (texture_width / 2.0F), (res.y / 2.0F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {}, 0.0F, blinking_color);
    src.width = -src.width;
    dst = {res.x - (half_screen_width / 2.0F) - (texture_width / 2.0F), (res.y / 2.0F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {}, 0.0F, blinking_color);

#define ART_COLOR_COUNT 40

    Color static const art_colors[ART_COLOR_COUNT] = {
        LIGHTGRAY, GRAY,      DARKGRAY,    YELLOW,   GOLD,     ORANGE,   PINK,         RED,          MAROON,      GREEN,
        LIME,      DARKGREEN, SKYBLUE,     BLUE,     DARKBLUE, PURPLE,   VIOLET,       DARKPURPLE,   BEIGE,       BROWN,
        DARKBROWN, WHITE,     BLACK,       BLANK,    MAGENTA,  RAYWHITE, SALMON,       DARKESTGRAY,  NEARBLACK,   NAYGREEN,
        NAYBLUE,   NAYBEIGE,  HACKERGREEN, REDBLACK, CYAN,     C64CYAN,  C64LIGHTBLUE, C64LIGHTGRAY, C64DARKGRAY, DAYLIGHT,
    };

    // Full screen rectangle with breathing effect
    F32 const breath_period    = 6.0F;  // Total time for one breath cycle in seconds
    F32 const breath_min_alpha = 0.0F;  // Minimum darkness
    F32 const breath_max_alpha = 0.2F;  // Maximum darkness

    // Use sin_f32 to get smooth -1 to 1 oscillation
    F32 const sin_time = math_sin_f32((2.0F * glm::pi<F32>()) * (time_get() / breath_period));
    // Remap from -1,1 to 0,1 and then to min,max alpha
    F32 const alpha = math_lerp_f32(breath_min_alpha, breath_max_alpha, (sin_time + 1.0F) * 0.5F);

    d2d_rectangle_rec({0, 0, res.x, res.y}, Fade(BLACK, alpha));  // Black with breathing alpha

    // AGAIN
    src.width = -src.width;
    dst       = {(half_screen_width / 2.0F) - (texture_width / 2.0F), (res.y / 2.0F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(WHITE, 0.9F), Fade(BEIGE, 0.65F)));
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F) - (texture_width / 2.0F), (res.y / 2.0F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(WHITE, 0.9F), Fade(BEIGE, 0.65F)));

    // AGAIN
    src.width = -src.width;
    dst       = {(half_screen_width / 2.0F) - (texture_width / 3.0F), (res.y / 2.0F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(MAGENTA, 0.2F), Fade(BEIGE, 0.65F)));
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F) - (texture_width / 1.5F), (res.y / 2.0F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(MAGENTA, 0.2F), Fade(BEIGE, 0.65F)));

    // AGAIN
    src.width = -src.width;
    dst       = {(half_screen_width / 2.0F) - (texture_width / 1.5F), (res.y / 2.0F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(RED, 0.1F), Fade(BEIGE, 0.05F)));
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F) - (texture_width / 3.0F), (res.y / 2.0F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(RED, 0.1F), Fade(BEIGE, 0.05F)));

    SZ const loop_count       = 50;
    F32 const start_perc_size = 0.50F;
    F32 perc_size             = start_perc_size;
    F32 const perc_step       = 0.025F;
    F32 const position_step_x = res.x / 10.0F;
    F32 const position_step_y = res.y / 10.0F;

    for (SZ i = 0; i < loop_count; i++) {
        perc_size += perc_step;
        if (perc_size >= 1.0F) { perc_size = start_perc_size; }

        Vector2 const position = {
            ((F32)(i % 10) * position_step_x) - (texture_height * 0.20F),
            ((F32)((F32)i / 10) * position_step_y) - (texture_height * 0.10F),
        };

        SZ const idx = i % 3;
        switch (idx) {
            case 0: {
                blinking_color = color_sync_blinking_regular(Fade(art_colors[i % ART_COLOR_COUNT], 0.5F), Fade(BEIGE, 0.25F));
            } break;
            case 1: {
                blinking_color = color_sync_blinking_slow(Fade(art_colors[i % ART_COLOR_COUNT], 0.5F), Fade(BEIGE, 0.25F));
            } break;
            case 2: {
                blinking_color = color_sync_blinking_very_slow(Fade(art_colors[i % ART_COLOR_COUNT], 0.5F), Fade(BEIGE, 0.25F));
            } break;
            default: {
                _unreachable_();
            }
        }

        src.width = -src.width;
        d2d_texture_pro(texture, src, {position.x, position.y, texture_width * perc_size, texture_height * perc_size}, {0}, 0.0F, blinking_color);
    }

    d2d_text_shadow(title_font, title, title_position, title_color, title_color_shadow, title_shadow_offset);

    // AGAIN
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F) - (texture_width * 1.7F), (res.y / 2.0F) - (texture_height / 1.1F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(ORANGE, 0.25F), Fade(BEIGE, 0.05F)));
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F) - (texture_width * 0.7F), (res.y / 2.0F) - (texture_height / 1.5F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_regular(Fade(CYAN, 0.25F), Fade(BEIGE, 0.05F)));

    // AGAIN
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F) - (texture_width * 1.7F / 1.0F), (res.y / 2.0F) - (texture_height / 1.1F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(GOLD, 0.25F), Fade(BEIGE, 0.05F)));
    src.width = -src.width;
    dst       = {res.x - (texture_width / 2.0F), res.y - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(CYAN, 0.25F), Fade(MAGENTA, 0.05F)));

    // AGAIN
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F), (res.y / 2.0F) - (texture_height / 1.1F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_regular(Fade(YELLOW, 0.25F), Fade(GREEN, 0.05F)));
    src.width = -src.width;
    dst       = {res.x - (texture_width / 1.25F), texture_height / 2.5F, texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(RED, 0.25F), Fade(CYAN, 0.05F)));

    // AGAIN
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F) - texture_width, (res.y / 2.0F) - (texture_height / 1.1F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_regular(Fade(PINK, 0.25F), Fade(CYAN, 0.05F)));
    src.width = -src.width;
    dst       = {0.0F, 0.0F, texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(ORANGE, 0.25F), Fade(CYAN, 0.05F)));

    // AGAIN
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F) - (texture_width * 1.7F / 1.0F), (res.y / 2.0F) - (texture_height / 1.1F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(MAGENTA, 0.25F), Fade(CYAN, 0.05F)));
    src.width = -src.width;
    dst       = {res.x - (texture_width / 2.0F), 0.0F, texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_regular(Fade(BLUE, 0.25F), Fade(MAGENTA, 0.05F)));

    // AGAIN
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F) - (texture_width * 1.7F / 1.0F), (res.y / 2.0F) - (texture_height / 1.1F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(DARKBLUE, 0.25F), Fade(CYAN, 0.05F)));
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F) - (texture_width * 0.7F / 1.0F), (res.y / 2.0F) - (texture_height / 1.5F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_regular(Fade(CYAN, 0.25F), Fade(MAGENTA, 0.05F)));

    // AGAIN
    src.width = -src.width;
    dst       = {(half_screen_width / 2.0F) - (texture_width * 1.1F / 1.0F), (res.y / 2.0F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(RED, 0.25F), Fade(MAGENTA, 0.05F)));
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F) - (texture_width * 1.1F / 1.0F), (res.y / 2.0F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_regular(Fade(RED, 0.25F), Fade(CYAN, 0.05F)));

    // AGAIN
    src.width = -src.width;
    dst       = {(half_screen_width / 2.0F) - (texture_width * 0.9F / 1.0F), (res.y / 2.0F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(BLUE, 0.25F), Fade(MAGENTA, 0.05F)));
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F) - (texture_width * 0.9F / 1.0F), (res.y / 2.0F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_regular(Fade(BLUE, 0.25F), Fade(MAGENTA, 0.05F)));

    // AGAIN
    src.width = -src.width;
    dst       = {res.x - (texture_width / 2.0F), 0.0F + (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(BLUE, 0.25F), Fade(CYAN, 0.05F)));
    src.width = -src.width;
    dst       = {res.x - (texture_width / 2.0F) - (texture_width * 0.9F / 1.0F), (res.y / 2.0F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(BLUE, 0.25F), Fade(MAGENTA, 0.05F)));

    // AGAIN
    src.width = -src.width;
    dst       = {(half_screen_width / 2.0F) - (texture_width * 0.7F / 1.0F), (res.y / 2.0F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_regular(Fade(YELLOW, 0.25F), Fade(BEIGE, 0.05F)));
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F) - (texture_width * 0.7F / 1.0F), (res.y / 2.0F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(YELLOW, 0.25F), Fade(BEIGE, 0.05F)));

    // AGAIN
    src.width = -src.width;
    dst       = {(half_screen_width / 2.0F) - (texture_width / 1.0F), (res.y / 1.5F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_regular(Fade(RED, 0.25F), Fade(BEIGE, 0.05F)));
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F) - (texture_width / 1.0F), (res.y / 1.5F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(RED, 0.25F), Fade(BEIGE, 0.05F)));

    // AGAIN
    src.width = -src.width;
    dst       = {(half_screen_width / 2.0F) - (texture_width / 1.0F), (res.y / 1.5F) - (texture_height / 1.5F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_slow(Fade(MAROON, 0.25F), Fade(BEIGE, 0.05F)));
    src.width = -src.width;
    dst       = {res.x - (half_screen_width / 2.0F) - (texture_width / 1.0F), (res.y / 1.5F) - (texture_height / 2.0F), texture_width, texture_height};
    d2d_texture_pro(texture, src, dst, {0}, 0.0F, color_sync_blinking_regular(Fade(ORANGE, 0.25F), Fade(BEIGE, 0.05F)));
}
