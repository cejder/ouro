#include "asset.hpp"
#include "audio.hpp"
#include "color.hpp"
#include "common.hpp"
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

struct State {
    Scene *scene;
    ScreenFadeType current_fade_type;
    EaseType current_ease_type;
    BOOL fade_active;
    F32 fade_duration;
} static s = {};


void static i_start_fade() {
    screen_fade_init(s.current_fade_type, s.fade_duration, g_render.accent_color, s.current_ease_type, nullptr);
    s.fade_active = true;
}

SCENE_INIT(fade_demo) {
    s.scene = scene;
}

SCENE_ENTER(fade_demo) {
    s.scene->clear_color = color_darken(NAYBEIGE, 0.75F);

    s.current_fade_type = SCREEN_FADE_TYPE_FADE_IN;
    s.current_ease_type = EASE_LINEAR;
    s.fade_duration     = 4.0F;
    s.fade_active       = false;

    i_start_fade();
}

SCENE_RESUME(fade_demo) {}

SCENE_EXIT(fade_demo) {}

SCENE_UPDATE(fade_demo) {
    unused(dtu);

    if (s.fade_active) {
        BOOL const finished = screen_fade_update(dt);
        if (finished) {
            audio_play(ACG_SFX, "hit.ogg");
            s.fade_active = false;
        }
    }

    if (c_console__enabled) { return; }

    // Cycle through fade types
    if (is_pressed(IA_YES)) {
        audio_play(ACG_SFX, "menu_selection.ogg");
        s.current_fade_type = (ScreenFadeType)((s.current_fade_type + 1) % SCREEN_FADE_TYPE_COUNT);
        i_start_fade();
    }

    if (is_pressed(IA_CONSOLE_DELETE)) {
        audio_play(ACG_SFX, "menu_selection.ogg");
        s.current_fade_type = (ScreenFadeType)((s.current_fade_type + SCREEN_FADE_TYPE_COUNT - 1) % SCREEN_FADE_TYPE_COUNT);
        i_start_fade();
    }

    // Cycle through ease types
    if (is_pressed(IA_MOVE_3D_JUMP)) {
        audio_play(ACG_SFX, "menu_selection.ogg");
        s.current_ease_type = (EaseType)((s.current_ease_type + 1) % EASE_COUNT);
        i_start_fade();
    }

    if (is_pressed(IA_NO)) {
        audio_play(ACG_SFX, "menu_selection.ogg");
        s.current_ease_type = (EaseType)((s.current_ease_type + EASE_COUNT - 1) % EASE_COUNT);
        i_start_fade();
    }

    // Restart current fade
    if (is_pressed(IA_TURN_3D_RIGHT)) {
        audio_play(ACG_SFX, "plop.ogg");
        i_start_fade();
    }

    // Adjust duration
    if (is_down(IA_MOVE_2D_UP)) {
        audio_play(ACG_SFX, "menu_movement.ogg");
        s.fade_duration = math_approach_f32(s.fade_duration, 10.0F, dt * 0.5F);
    }
    if (is_down(IA_MOVE_2D_DOWN)) {
        audio_play(ACG_SFX, "menu_movement.ogg");
        s.fade_duration = math_approach_f32(s.fade_duration, 0.1F, dt * 0.5F);
    }

    if (is_pressed(IA_OPEN_OVERLAY_MENU)) {
        scenes_push_overlay_scene(SCENE_OVERLAY_MENU);
    }
}

SCENE_DRAW(fade_demo) {
    RMODE_BEGIN(RMODE_2D_DBG) {
        Vector2 const res         = render_get_render_resolution();;
        AFont *font               = asset_get_font("GoMono", ui_font_size(2.0));
        Vector2 const shadow_size = ui_shadow_offset(0.1F, 0.1F);

        F32 const padding     = ui_scale_x(1);
        F32 const text_area_x = ui_scale_x(3);
        F32 const text_area_y = text_area_x;

        // Create all text strings first
        C8 const *fade_name = screen_fade_type_to_cstr(s.current_fade_type);
        C8 const *ease_name = ease_to_cstr(s.current_ease_type);

        String *fade_type_text   = TS("\\ouc{#d1bc8aff}Fade Type: \\ouc{#ffffffff}%s", fade_name);
        String *ease_type_text   = TS("\\ouc{#d1bc8aff}Ease Type: \\ouc{#ffffffff}%s", ease_name);
        String *duration_text    = TS("\\ouc{#d1bc8aff}Duration:  \\ouc{#ffffffff}%.2fs", s.fade_duration);
        C8 const *controls_text  =    "\\ouc{#d1bc8aff}Controls:";
        C8 const *enter_text     =    "\\ouc{#00ffffff}Enter     \\ouc{#ffffffff}- Next Fade Type";
        C8 const *delete_text    =    "\\ouc{#00ffffff}Delete    \\ouc{#ffffffff}- Prev Fade Type";
        C8 const *space_text     =    "\\ouc{#00ffffff}Space     \\ouc{#ffffffff}- Next Ease Type";
        C8 const *backspace_text =    "\\ouc{#00ffffff}Backspace \\ouc{#ffffffff}- Prev Ease Type";
        C8 const *r_text         =    "\\ouc{#00ffffff}R         \\ouc{#ffffffff}- Restart Fade";
        C8 const *ed_text        =    "\\ouc{#00ffffff}E/D       \\ouc{#ffffffff}- Adjust Duration";

        // Measure all text dimensions
        Vector2 const fade_type_size = measure_text_ouc(font, fade_type_text->c);
        Vector2 const ease_type_size = measure_text_ouc(font, ease_type_text->c);
        Vector2 const duration_size  = measure_text_ouc(font, duration_text->c);
        Vector2 const controls_size  = measure_text_ouc(font, controls_text);
        Vector2 const enter_size     = measure_text_ouc(font, enter_text);
        Vector2 const delete_size    = measure_text_ouc(font, delete_text);
        Vector2 const space_size     = measure_text_ouc(font, space_text);
        Vector2 const backspace_size = measure_text_ouc(font, backspace_text);
        Vector2 const r_size         = measure_text_ouc(font, r_text);
        Vector2 const ed_size        = measure_text_ouc(font, ed_text);

        // Find the maximum width
        F32 max_text_width = 0.0F;
        max_text_width = glm::max(max_text_width, fade_type_size.x);
        max_text_width = glm::max(max_text_width, ease_type_size.x);
        max_text_width = glm::max(max_text_width, duration_size.x);
        max_text_width = glm::max(max_text_width, controls_size.x);
        max_text_width = glm::max(max_text_width, enter_size.x);
        max_text_width = glm::max(max_text_width, delete_size.x);
        max_text_width = glm::max(max_text_width, space_size.x);
        max_text_width = glm::max(max_text_width, backspace_size.x);
        max_text_width = glm::max(max_text_width, r_size.x);
        max_text_width = glm::max(max_text_width, ed_size.x);

        // Calculate total height
        F32 const total_text_height =
            fade_type_size.y + ease_type_size.y + duration_size.y                                 + (padding)         +
            controls_size.y                                                                       + (padding * 0.25F) +
            enter_size.y + delete_size.y + space_size.y + backspace_size.y + r_size.y + ed_size.y + (padding * 2);

        // Draw background rectangle with padding
        Color const bg_color = Fade(NAYGREEN, 0.5F);
        Rectangle const text_bg_rec = {
            text_area_x - padding,
            text_area_y - padding,
            max_text_width + (padding * 2),
            total_text_height
        };
        d2d_rectangle_rec(text_bg_rec, bg_color);

        F32 cur_y = text_area_y;
        Color const text_col = WHITE;
        Color const shadow_col = BLACK;

        // Draw all text
        d2d_text_ouc_shadow(font, fade_type_text->c, {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += fade_type_size.y;
        d2d_text_ouc_shadow(font, ease_type_text->c, {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += ease_type_size.y;
        d2d_text_ouc_shadow(font, duration_text->c,  {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += duration_size.y + padding;
        d2d_text_ouc_shadow(font, controls_text,     {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += controls_size.y + padding * 0.25F;
        d2d_text_ouc_shadow(font, enter_text,        {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += enter_size.y;
        d2d_text_ouc_shadow(font, delete_text,       {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += delete_size.y;
        d2d_text_ouc_shadow(font, space_text,        {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += space_size.y;
        d2d_text_ouc_shadow(font, backspace_text,    {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += backspace_size.y;
        d2d_text_ouc_shadow(font, r_text,            {text_area_x, cur_y}, text_col, shadow_col, shadow_size); cur_y += r_size.y;
        d2d_text_ouc_shadow(font, ed_text,           {text_area_x, cur_y}, text_col, shadow_col, shadow_size);

        // Progress bar in lower half (75% down from top)
        if (s.fade_active) {
            font = asset_get_font("GoMono", ui_font_size(5));

            F32 const progress      = screen_fade_get_elapsed_perc();
            F32 const ease_progress = ease_use(s.current_ease_type, progress, 0.0F, 1.0F, 1.0F);
            F32 const bar_width     = res.x;
            F32 const bar_height    = ui_scale_y(7.5);
            F32 const bar_x         = (res.x - bar_width) * 0.25F;
            F32 const bar_y         = res.y - (bar_height + ui_scale_x(5));

            Rectangle const bg_rec            = {bar_x, bar_y, bar_width, bar_height};
            Rectangle const progress_rec      = {bar_x, bar_y, bar_width * progress, bar_height};
            Rectangle const ease_progress_rec = {bar_x, bar_y, bar_width * ease_progress, bar_height};

            d2d_rectangle_rec(bg_rec, color_darken(NEARBLACK, 0.5F));
            d2d_rectangle_rec(progress_rec, Fade(EVAFATORANGE, 0.5F));
            d2d_rectangle_rec(ease_progress_rec, Fade(EVAGREEN,0.5F));

            C8 const *progress_text          = TS("%.0f%%", progress * 100.0F)->c;
            Vector2 const progress_text_size = measure_text(font, progress_text);
            Vector2 const progress_text_pos  = {
                bar_x + ((bar_width - progress_text_size.x) * 0.5F),
                bar_y + ((bar_height - progress_text_size.y) * 0.5F)
            };
            d2d_text_ouc_shadow(font, progress_text, progress_text_pos, WHITE, BLACK, shadow_size);
        }
    }
    RMODE_END;

    RMODE_BEGIN(RMODE_2D_HUD) {
        screen_fade_draw_2d_hud();
    }
    RMODE_END;
}
