#include "asset.hpp"
#include "audio.hpp"
#include "cvar.hpp"
#include "fade.hpp"
#include "input.hpp"
#include "render.hpp"
#include "scene.hpp"

#define LOGO_FADE_OUT_DURATION 1.25F
#define LOGO_FADE_IN_DURATION 4.0F

struct State {
    Scene *scene;
    BOOL played_intro_sound;
    BOOL loading_fade_out_finished;
} static s = {};

SCENE_INIT(intro) {
    s.scene = scene;
}

SCENE_ENTER(intro) {
    s.scene->clear_color = BLACK;

    s.played_intro_sound = false;
    s.loading_fade_out_finished = false;

    audio_set_pan(ACG_VOICE, 0.20F);
    audio_set_pitch(ACG_VOICE, 0.60F);
    audio_attach_dsp_by_type(ACG_VOICE, FMOD_DSP_TYPE_SFXREVERB);

    // Init loading screen fade out
    screen_fade_init(SCREEN_FADE_TYPE_FADE_OUT, LOGO_FADE_OUT_DURATION, WHITE, EASE_IN_OUT_SINE, asset_get_texture("loading.jpg"));
}

SCENE_RESUME(intro) {}

SCENE_EXIT(intro) {
    audio_reset_all();
}

SCENE_UPDATE(intro) {
    unused(dtu);

    if (!s.played_intro_sound) {
        audio_play(ACG_VOICE, "reverse_yawn.ogg");
        s.played_intro_sound = true;
    }

    if (!s.loading_fade_out_finished) {
        if (screen_fade_update(dt)) {
            s.loading_fade_out_finished = true;
            // When loading screen fade is done, we start the logo fade in
            screen_fade_init(SCREEN_FADE_TYPE_FADE_IN, LOGO_FADE_IN_DURATION, WHITE, EASE_OUT_QUAD, asset_get_texture("felodese.png"));
        }
    }

    if (s.loading_fade_out_finished) {
        if (screen_fade_update(dt)) {
            scenes_set_scene(SCENE_MENU); }
    }

    if (c_console__enabled) { return; }
    if (is_down(IA_OPEN_OVERLAY_MENU) || is_down(IA_YES) || is_down(IA_NO) || is_mouse_down(I_MOUSE_LEFT) || is_mouse_down(I_MOUSE_RIGHT)) {
        g_screen_fade.finished = true; // Skip the intro.
    }
}

SCENE_DRAW(intro) {
    RMODE_BEGIN(RMODE_2D_HUD) {
        screen_fade_draw_2d_hud();
    }
    RMODE_END;
}
