#include "asset.hpp"
#include "audio.hpp"
#include "color.hpp"
#include "common.hpp"
#include "console.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "fade.hpp"
#include "input.hpp"
#include "math.hpp"
#include "message.hpp"
#include "particles_2d.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "string.hpp"

#include <raymath.h>

#define RECORDING_DURATION 5.0F

enum PlaybackState : U8 {
    PLAYBACK_STATE_RECORDING,
    PLAYBACK_STATE_PLAYING,
    PLAYBACK_STATE_PAUSED,
};

struct State {
    F32 size_multiplier;
    Scene *scene;
    MouseTape mrec;
    PlaybackState state;
    F32 timer;
} static s = {};

SCENE_INIT(particles_demo) {
    s.scene = scene;
    s.size_multiplier = 2.0F;
    s.state = PLAYBACK_STATE_PAUSED;
    s.timer = 0.0F;
    // mouse_recorder_init(&s.mrec, "particles_demo");
}

SCENE_ENTER(particles_demo) {
    mouse_recorder_load(&s.mrec, "particles_demo");

    screen_fade_init(SCREEN_FADE_TYPE_FADE_OUT, SCREEN_FADE_DEFAULT_DURATION, BLACK, EASE_IN_OUT_SINE, nullptr);
}

SCENE_RESUME(particles_demo) {}

SCENE_EXIT(particles_demo) {}

SCENE_UPDATE(particles_demo) {
    unused(dtu);

    screen_fade_update(dt);

    if (s.state != PLAYBACK_STATE_PAUSED) { s.timer += dt; }

    if (s.state == PLAYBACK_STATE_RECORDING) {
        mouse_recorder_record_frame(&s.mrec);
    } else if (s.state == PLAYBACK_STATE_PLAYING) {
        // Check if playback finished - loop back to start
        if (s.timer * 60.0F >= (F32)s.mrec.frames.count) {
            s.timer = 0.0F;
            input_play_mouse_tape(&s.mrec);
        }
    }

    // R to start recording
    if (is_pressed(IA_TURN_3D_RIGHT)) {
        s.state = PLAYBACK_STATE_RECORDING;
        s.timer = 0.0F;
        input_stop_mouse_tape();
        mouse_recorder_reset(&s.mrec);
    }

    // SPACE to toggle playback/pause
    if (is_pressed(IA_MOVE_3D_JUMP)) {
        if (s.state == PLAYBACK_STATE_RECORDING) {
            mouse_recorder_save(&s.mrec);
            s.state = PLAYBACK_STATE_PLAYING;
            s.timer = 0.0F;
            input_play_mouse_tape(&s.mrec);
        } else if (s.state == PLAYBACK_STATE_PLAYING) {
            s.state = PLAYBACK_STATE_PAUSED;
            input_pause_mouse_tape();
        } else if (s.state == PLAYBACK_STATE_PAUSED) {
            s.state = PLAYBACK_STATE_PLAYING;
            input_play_mouse_tape(&s.mrec);
        }
    }

    if (c_console__enabled) { return; }

    if (is_pressed(IA_YES)) {
        audio_play(ACG_SFX, "menu_selection.ogg");
        // TODO:
    }

    if (is_pressed_or_repeat(IA_MOVE_2D_UP)) {
        audio_play(ACG_SFX, "menu_selection.ogg");
        s.size_multiplier += 0.1F;
    }

    if (is_pressed_or_repeat(IA_MOVE_2D_DOWN)) {
        audio_play(ACG_SFX, "menu_selection.ogg");
        s.size_multiplier -= 0.1F;
        s.size_multiplier  = glm::max(0.1F, s.size_multiplier);
    }

    Vector2 const mouse_pos = input_get_mouse_position();

    if (is_down(IA_DBG_LIGHT_1)) {
        Rectangle const spawn_rect = {mouse_pos.x - 25.0F, mouse_pos.y - 25.0F, 50.0F, 50.0F};
        particles2d_add_explosion(spawn_rect, ORANGE, RED, s.size_multiplier, 150);
    }
    if (is_down(IA_DBG_LIGHT_2) || is_mouse_down(I_MOUSE_LEFT)) {
        Rectangle const spawn_rect = {mouse_pos.x - 20.0F, mouse_pos.y - 10.0F, 40.0F, 20.0F};
        particles2d_add_smoke(spawn_rect, GOLD, RED, s.size_multiplier, 100);
    }
    if (is_down(IA_DBG_LIGHT_3)) {
        Rectangle const spawn_rect = {mouse_pos.x - 40.0F, mouse_pos.y - 40.0F, 80.0F, 80.0F};
        particles2d_add_sparkle(spawn_rect, BEIGE, GOLD, s.size_multiplier, 80);
    }
    if (is_down(IA_DBG_LIGHT_4)) {
        Rectangle const spawn_rect = {mouse_pos.x - 25.0F, mouse_pos.y - 15.0F, 50.0F, 20.0F};
        particles2d_add_fire(spawn_rect, GOLD, RED, s.size_multiplier, 120);
    }
    if (is_down(IA_DBG_LIGHT_5)) {
        Rectangle const spawn_rect = {mouse_pos.x - 80.0F, mouse_pos.y - 80.0F, 160.0F, 160.0F};
        particles2d_add_spiral(spawn_rect, SKYBLUE, PURPLE, s.size_multiplier, 120);
    }

    if (is_pressed(IA_OPEN_OVERLAY_MENU)) {
        scenes_push_overlay_scene(SCENE_OVERLAY_MENU);
    }
}

SCENE_DRAW(particles_demo) {
    RMODE_BEGIN(RMODE_2D_DBG) {
        AFont *font               = asset_get_font("GoMono", ui_font_size(2.0));
        Vector2 const shadow_size = ui_shadow_offset(0.1F, 0.1F);

        F32 const padding     = ui_scale_x(1);
        F32 const text_area_x = ui_scale_x(3);
        F32 const text_area_y = text_area_x;

        String *h0 = TS("\\ouc{#d1bc8aff}PARTICLES DEMO   \\ouc{#81bc8aff}(MAX: %d)", PARTICLES_2D_MAX);
        String *h1 = TS("\\ouc{#d1bc8aff}Size Multiplier: \\ouc{#ffffffff}%.1f", s.size_multiplier);
        String *h2 = TS("\\ouc{#d1bc8aff}Active Textures: \\ouc{#ffffffff}%zu/32", g_particles2d.textures.count);
        C8 const *state_color = nullptr;
        C8 const *state_text  = nullptr;
        F32 display_time      = F32_MAX;

        if (s.state == PLAYBACK_STATE_RECORDING) {
            state_color  = "#ff0000ff";
            state_text   = "RECORDING";
            display_time = s.timer;
        } else if (s.state == PLAYBACK_STATE_PLAYING) {
            state_color  = "#00ff00ff";
            state_text   = "PLAYING";
            display_time = glm::max(0.0F, ((F32)s.mrec.frames.count / 60.0F) - s.timer);
        } else {
            state_color  = "#ffff00ff";
            state_text   = "PAUSED";
            display_time = glm::max(0.0F, ((F32)s.mrec.frames.count / 60.0F) - s.timer);
        }

        String *h3 = TS("\\ouc{#d1bc8aff}Mouse: \\ouc{%s}%s \\ouc{#ffffffff}(%.2fs)",
                        state_color, state_text, display_time);

        C8 const *controls_text = "\\ouc{#d1bc8aff}Controls:";
        C8 const *up_text       = "\\ouc{#00ffffff}E         \\ouc{#ffffffff}- Increase size";
        C8 const *down_text     = "\\ouc{#00ffffff}D         \\ouc{#ffffffff}- Decrease size";
        C8 const *rec_text      = "\\ouc{#00ffffff}R         \\ouc{#ffffffff}- Record tape";
        C8 const *effects_text  = "\\ouc{#d1bc8aff}Effects:";
        C8 const *effect1_text  = "\\ouc{#00ffffff}1         \\ouc{#ffffffff}- Explosion";
        C8 const *effect2_text  = "\\ouc{#00ffffff}2         \\ouc{#ffffffff}- Smoke Trail";
        C8 const *effect3_text  = "\\ouc{#00ffffff}3         \\ouc{#ffffffff}- Sparkle";
        C8 const *effect4_text  = "\\ouc{#00ffffff}4         \\ouc{#ffffffff}- Fire";
        C8 const *effect5_text  = "\\ouc{#00ffffff}5         \\ouc{#ffffffff}- Magic Spiral";

        // Measure all text dimensions
        Vector2 const h0_size       = measure_text_ouc(font, h0->c);
        Vector2 const h1_size       = measure_text_ouc(font, h1->c);
        Vector2 const h2_size       = measure_text_ouc(font, h2->c);
        Vector2 const h3_size       = measure_text_ouc(font, h3->c);
        Vector2 const controls_size = measure_text_ouc(font, controls_text);
        Vector2 const up_size       = measure_text_ouc(font, up_text);
        Vector2 const down_size     = measure_text_ouc(font, down_text);
        Vector2 const rec_size      = measure_text_ouc(font, rec_text);
        Vector2 const effects_size  = measure_text_ouc(font, effects_text);
        Vector2 const effect1_size  = measure_text_ouc(font, effect1_text);
        Vector2 const effect2_size  = measure_text_ouc(font, effect2_text);
        Vector2 const effect3_size  = measure_text_ouc(font, effect3_text);
        Vector2 const effect4_size  = measure_text_ouc(font, effect4_text);
        Vector2 const effect5_size  = measure_text_ouc(font, effect5_text);

        // Find the maximum width
        F32 max_text_width = 0.0F;
        max_text_width     = glm::max(max_text_width, h0_size.x);
        max_text_width     = glm::max(max_text_width, h1_size.x);
        max_text_width     = glm::max(max_text_width, h2_size.x);
        max_text_width     = glm::max(max_text_width, h3_size.x);
        max_text_width     = glm::max(max_text_width, controls_size.x);
        max_text_width     = glm::max(max_text_width, up_size.x);
        max_text_width     = glm::max(max_text_width, down_size.x);
        max_text_width     = glm::max(max_text_width, rec_size.x);
        max_text_width     = glm::max(max_text_width, effects_size.x);
        max_text_width     = glm::max(max_text_width, effect1_size.x);
        max_text_width     = glm::max(max_text_width, effect2_size.x);
        max_text_width     = glm::max(max_text_width, effect3_size.x);
        max_text_width     = glm::max(max_text_width, effect4_size.x);
        max_text_width     = glm::max(max_text_width, effect5_size.x);

        // Calculate total height
        F32 const total_text_height =
            h0_size.y + h1_size.y + h2_size.y + h3_size.y                                      + (padding)         +
            controls_size.y                                                                    + (padding * 0.25F) +
            up_size.y + down_size.y + rec_size.y                                               + (padding * 0.5F)  +
            effects_size.y                                                                     + (padding * 0.25F) +
            effect1_size.y + effect2_size.y + effect3_size.y + effect4_size.y + effect5_size.y + (padding * 2);

        // Draw background rectangle with padding
        Color const bg_color = Fade(NAYGREEN, 0.5F);
        Rectangle const text_bg_rec = {
            text_area_x    -  padding,
            text_area_y    -  padding,
            max_text_width + (padding * 2),
            total_text_height
        };
        d2d_rectangle_rec(text_bg_rec, bg_color);

        F32 cur_y              = text_area_y;
        Color const text_col   = WHITE;
        Color const shadow_col = BLACK;

        // Draw all text
        d2d_text_ouc_shadow(font, h0->c,         {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += h0_size.y;
        d2d_text_ouc_shadow(font, h1->c,         {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += h1_size.y;
        d2d_text_ouc_shadow(font, h2->c,         {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += h2_size.y;
        d2d_text_ouc_shadow(font, h3->c,         {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += h3_size.y        + padding;
        d2d_text_ouc_shadow(font, controls_text, {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += controls_size.y  + (padding * 0.25F);
        d2d_text_ouc_shadow(font, up_text,       {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += up_size.y;
        d2d_text_ouc_shadow(font, down_text,     {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += down_size.y;
        d2d_text_ouc_shadow(font, rec_text,      {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += rec_size.y       + (padding * 0.5F);
        d2d_text_ouc_shadow(font, effects_text,  {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += effects_size.y   + (padding * 0.25F);
        d2d_text_ouc_shadow(font, effect1_text,  {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += effect1_size.y;
        d2d_text_ouc_shadow(font, effect2_text,  {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += effect2_size.y;
        d2d_text_ouc_shadow(font, effect3_text,  {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += effect3_size.y;
        d2d_text_ouc_shadow(font, effect4_text,  {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += effect4_size.y;
        d2d_text_ouc_shadow(font, effect5_text,  {text_area_x, cur_y}, text_col, shadow_col, shadow_size);
    } RMODE_END;

    RMODE_BEGIN(RMODE_2D) {
        particles2d_draw();
    } RMODE_END;

    RMODE_BEGIN(RMODE_2D_HUD) {
        screen_fade_draw_2d_hud();
    } RMODE_END;
}
