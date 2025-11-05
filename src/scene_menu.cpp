#include "asset.hpp"
#include "audio.hpp"
#include "color.hpp"
#include "console.hpp"
#include "core.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "fade.hpp"
#include "math.hpp"
#include "menu.hpp"
#include "particles_2d.hpp"
#include "profiler.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "scene_extra.hpp"
#include "string.hpp"

struct State {
    Scene *scene;
    Menu menu;
    C8 const *title;
} static s = {};

SCENE_INIT(menu) {
    s.scene = scene;
    s.title = OURO_TITLE;

    menu_init_with_general_menu_defaults(&s.menu,
                                         menu_extra_menu_func_get_entry_count(),
                                         menu_extra_menu_func_get_label,
                                         nullptr,
                                         menu_extra_menu_func_get_desc,
                                         nullptr,
                                         nullptr,
                                         nullptr,
                                         menu_extra_menu_func_yes,
                                         nullptr,
                                         (void *)(&s.menu.selected_entry));
}

SCENE_ENTER(menu) {
    s.scene->clear_color = SCENE_DEFAULT_CLEAR_COLOR;

    audio_set_pitch(ACG_VOICE, 0.75F);

    audio_attach_dsp_by_type(ACG_MUSIC, FMOD_DSP_TYPE_SFXREVERB);
    audio_attach_dsp_by_type(ACG_SFX, FMOD_DSP_TYPE_SFXREVERB);
    audio_attach_dsp_by_type(ACG_VOICE, FMOD_DSP_TYPE_SFXREVERB);

    audio_play(ACG_MUSIC, "temples.ogg");
    audio_play(ACG_VOICE, "the_voice.ogg");

    screen_fade_init(SCREEN_FADE_TYPE_FADE_OUT, SCREEN_FADE_DEFAULT_DURATION, g_render.accent_color, EASE_IN_OUT_SINE, nullptr);
}

SCENE_RESUME(menu) {
    screen_fade_init(SCREEN_FADE_TYPE_FADE_OUT, SCREEN_FADE_QUICK_DEFAULT_DURATION, g_render.accent_color, EASE_IN_OUT_SINE, nullptr);
}

SCENE_EXIT(menu) {
    audio_reset_all();
}

SCENE_UPDATE(menu) {
    unused(dtu);

    menu_update(&s.menu);
    screen_fade_update(dt);

    if (c_console__enabled) { return; }
}

SCENE_DRAW(menu) {
    Vector2 const res = render_get_render_resolution();

    RMODE_BEGIN(RMODE_2D) {
        F32 const padding         = 50.0F;
        Rectangle const full_rec  = {0.0F, 0.0F, res.x, res.y};
        Rectangle const right_rec = {s.menu.bounds_internal.x - padding, 0.0F, res.x - s.menu.bounds_internal.x + padding, res.y};
        d2d_rectangle_rec(full_rec, color_sync_blinking_slow(SALMON, NAYBEIGE));
        d2d_rectangle_rec(right_rec, Fade(BLACK, 0.75F));
        AFont *title_font            = asset_get_font("GoMono", ui_font_size(5.0F));
        Vector2 const title_dim      = measure_text(title_font, s.title);
        Vector2 const title_position = {(res.x / 2.0F) - (title_dim.x / 2.0F), (res.y / 2.0F) - (title_dim.y / 2.0F)};
        scene_extra_draw_menu_art(asset_get_texture("dudos.png"), title_font, s.title, title_position, NAYBEIGE, BLACK, ui_shadow_offset(0.1F, 0.25F));
    }
    RMODE_END;

    RMODE_BEGIN(RMODE_2D_HUD) {
        particles2d_draw();
        menu_draw_2d_hud(&s.menu);

        AFont *font                   = asset_get_font("GoMono", ui_font_size(2.5F));
        Vector2 const text_dimensions = measure_text(font, g_core.version_info.c);
        d2d_text_shadow(font, g_core.version_info.c, {ui_scale_x(1.0F), res.y - text_dimensions.y - ui_scale_y(1.0F)}, NAYBEIGE, BLACK, ui_shadow_offset(0.1F, 0.1F));

        screen_fade_draw_2d_hud();
    }
    RMODE_END;

    RMODE_BEGIN(RMODE_2D_DBG) {
        menu_draw_2d_dbg(&s.menu);
    }
    RMODE_END;
}
