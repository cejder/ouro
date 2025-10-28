#include "asset.hpp"
#include "color.hpp"
#include "console.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "ease.hpp"
#include "fade.hpp"
#include "input.hpp"
#include "math.hpp"
#include "message.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "string.hpp"

#include <raymath.h>

#define BALL_SIZE_PERC 0.5
#define EASE_DURATION 5.0F

struct IEaseBall {
    F32 pos_x;
    F32 start_x;
    F32 end_x;
    C8 const *ease_name;
    EaseType ease_type;
};

struct State {
    Scene *scene;
    IEaseBall ease_balls[EASE_COUNT];
    Alarm ease_timer;
} static s = {};

void static i_reset_ease(Vector2 res) {
    for (SZ i = 0; i < EASE_COUNT; ++i) {
        IEaseBall *ball = &s.ease_balls[i];
        ball->start_x   = 0.0F;
        ball->end_x     = res.x;
        ball->pos_x     = ball->start_x;
        ball->ease_name = ease_to_cstr((EaseType)i);
        ball->ease_type = (EaseType)i;
    }

    alarm_start(&s.ease_timer, EASE_DURATION, true);
}

SCENE_INIT(ease_demo) {
    s.scene = scene;
}

SCENE_ENTER(ease_demo) {
    i_reset_ease(render_get_render_resolution());

    screen_fade_init(SCREEN_FADE_TYPE_FADE_OUT, SCREEN_FADE_DEFAULT_DURATION, BLACK, EASE_IN_OUT_SINE, nullptr);
}

SCENE_RESUME(ease_demo) {}

SCENE_EXIT(ease_demo) {}

SCENE_UPDATE(ease_demo) {
    unused(dtu);

    Vector2 static last_res = {};
    Vector2 const res       = render_get_render_resolution();
    if (res.x != last_res.x || res.y != last_res.y) {
        last_res = res;
        i_reset_ease(res);
    }

    alarm_tick(&s.ease_timer, dt);
    screen_fade_update(dt);

    F32 const gamepad_raxis_x = GetGamepadAxisMovement(input_get_main_gamepad_idx(), GAMEPAD_AXIS_RIGHT_X);
    if (math_abs_f32(gamepad_raxis_x) > GAMEPAD_AXIS_DEADZONE) {
        F32 const normalized_raxis_x = (gamepad_raxis_x + 1.0F) * 0.5F;
        s.ease_timer.elapsed = normalized_raxis_x * EASE_DURATION;
    }

    for (auto &ball : s.ease_balls) {
        F32 const ball_delta = ball.end_x - ball.start_x;
        ball.pos_x = ease_use(ball.ease_type, alarm_get_progress(&s.ease_timer), ball.start_x, ball_delta, s.ease_timer.duration);
    }

    if (c_console__enabled) { return; }
    if (is_pressed(IA_OPEN_OVERLAY_MENU)) { scenes_push_overlay_scene(SCENE_OVERLAY_MENU); }
}

SCENE_DRAW(ease_demo) {
    RMODE_BEGIN(RMODE_2D) {
        Vector2 const res                 = render_get_render_resolution();
        AFont *font                       = asset_get_font("GoMono", ui_font_size(1.0));
        F32 const vertical_padding        = res.y / (F32)EASE_COUNT;
        F32 current_y                     = vertical_padding / 2.0F;
        F32 const text_horizontal_padding = (F32)font->font_size;
        F32 const tooltip_padding         = ui_scale_x(0.1F);
        Vector2 const mouse_position      = input_get_mouse_position();
        Vector2 const shadow_size         = ui_shadow_offset(0.065F, 0.075F);
        F32 const ball_size               = ui_scale_x(BALL_SIZE_PERC);

        for (SZ i = 0; i < EASE_COUNT; ++i) {
            IEaseBall *ball        = &s.ease_balls[i];
            F32 const progress     = (ball->pos_x - ball->start_x) / (ball->end_x - ball->start_x);
            Color const line_color = color_lerp(OFFCOLOR, ONCOLOR, progress);
            Vector2 const p1       = {ball->start_x, current_y};
            Vector2 const p2       = {ball->pos_x, current_y};
            C8 const *text         = TS("\\ouc{#ffffffff}%02zu: \\ouc{#d1bc8aff}%s", i, ball->ease_name)->c;

            d2d_line_ex(p1, p2, ball_size * 2.0F, line_color);
            d2d_circle({ball->pos_x, current_y}, ball_size, WHITE);
            d2d_text_ouc_shadow(font, text, {ball->start_x + text_horizontal_padding, current_y - ((F32)font->font_size / 2)}, WHITE, BLACK, shadow_size);

            if (CheckCollisionPointCircle(mouse_position, {ball->pos_x, current_y}, ball_size)) {
                C8 const *tooltip                = TS("%.2f%%", progress * 100)->c;
                Vector2 const tooltip_dimensions = measure_text(font, tooltip);
                Vector2 const tooltip_position   = Vector2Add(mouse_position, {15.0F, -15.0F});
                Rectangle const tooltip_bg_rec   = {
                    tooltip_position.x - tooltip_padding,
                    tooltip_position.y - tooltip_padding,
                    tooltip_dimensions.x + (tooltip_padding * 2.0F),
                    tooltip_dimensions.y + (tooltip_padding * 2.0F),
                };

                d2d_rectangle_rec(tooltip_bg_rec, BLACK);
                d2d_text(font, tooltip, tooltip_position, line_color);
            }

            current_y += vertical_padding;
        }
    }
    RMODE_END;

    RMODE_BEGIN(RMODE_2D_HUD) {
        screen_fade_draw_2d_hud();
    }
    RMODE_END;
}
