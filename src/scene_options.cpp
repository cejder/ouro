#include "asset.hpp"
#include "color.hpp"
#include "console.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "fade.hpp"
#include "input.hpp"
#include "menu.hpp"
#include "particles_2d.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "scene_extra.hpp"

struct State {
    Scene *scene;
    Menu menu;
    C8 const *title;
} static s = {};

SCENE_INIT(options) {
    s.scene = scene;
    s.title = "OPTIONS";

    menu_init_with_option_menu_defaults(&s.menu,
                                        menu_extra_option_func_get_entry_count(),
                                        menu_extra_option_func_get_label,
                                        nullptr,
                                        menu_extra_option_func_get_desc,
                                        nullptr,
                                        menu_extra_option_func_get_value,
                                        nullptr,
                                        menu_extra_option_func_yes,
                                        menu_extra_option_func_change,
                                        (void *)(&s.menu.selected_entry));
}

SCENE_ENTER(options) {
    s.scene->clear_color = SCENE_DEFAULT_CLEAR_COLOR;

    screen_fade_init(SCREEN_FADE_TYPE_FADE_OUT, SCREEN_FADE_QUICK_DEFAULT_DURATION, g_render.accent_color, EASE_IN_OUT_SINE, nullptr);
}

SCENE_RESUME(options) {}

SCENE_EXIT(options) {}

SCENE_UPDATE(options) {
    unused(dt);
    unused(dtu);

    menu_update(&s.menu);
    screen_fade_update(dt);

    if (c_console__enabled) { return; }
    if (is_pressed(IA_OPEN_OVERLAY_MENU) || is_pressed(IA_NO)) {
        // We need to make sure that we handle the case where we call the OPTIONS scene from the OVERLAY MENU scene and not the MENU scene.
        // If that is the case, we can't just pop the overlay scene, we just replace the current overlay scene with OVERLAY MENU.
        // This might bite us in the ass if there are other exceptions but that is an issue for fUTUREbOY3k2 aka me.
        SceneType const current_scene_type = g_scenes.current_scene_type;
        if (current_scene_type == SCENE_MENU) {
            scenes_pop_overlay_scene(true, false);
        } else {
            scenes_push_overlay_scene(SCENE_OVERLAY_MENU);
        }
    }
}

SCENE_DRAW(options) {
    Vector2 const res = render_get_render_resolution();

    RMODE_BEGIN(RMODE_2D) {
        F32 const padding         = 50.0F;
        Rectangle const full_rec  = {0.0F, 0.0F, res.x, res.y};
        Rectangle const right_rec = {s.menu.bounds_internal.x - padding, 0.0F, res.x - s.menu.bounds_internal.x + padding, res.y};
        d2d_rectangle_rec(full_rec, color_sync_blinking_slow(BROWN, NAYBEIGE));
        d2d_rectangle_rec(right_rec, Fade(BLACK, 0.75F));
        AFont *title_font            = asset_get_font("GoMono", ui_font_size(5.0F));
        Vector2 const title_dim      = measure_text(title_font, s.title);
        Vector2 const title_position = {(res.x / 2.0F) - (title_dim.x / 2.0F), (res.y / 2.0F) - (title_dim.y / 2.0F)};
        scene_extra_draw_menu_art(asset_get_texture("dudas.png"), title_font, s.title, title_position, NAYBEIGE, BLACK, ui_shadow_offset(0.1F, 0.25F));
    }
    RMODE_END;

    RMODE_BEGIN(RMODE_2D_HUD) {
        particles2d_draw();
        menu_draw_2d_hud(&s.menu);
        screen_fade_draw_2d_hud();
    }
    RMODE_END;

    RMODE_BEGIN(RMODE_2D_DBG) {
        menu_draw_2d_dbg(&s.menu);
    }
    RMODE_END;
}
