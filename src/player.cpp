#include "player.hpp"
#include "asset.hpp"
#include "audio.hpp"
#include "cvar.hpp"
#include "input.hpp"
#include "math.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "world.hpp"

#include <raymath.h>
#include <glm/gtc/type_ptr.hpp>

void player_init(Player *p, Vector3 position, Vector3 target) {
    p->camera3d          = c3d_get_default();
    p->camera3d.position = position;
    p->camera3d.target   = target;
    p->previous_position = position;
    p->debug_icon        = asset_get_texture("you_are_here.png");
}

void player_update(Player *p, World *world, F32 dt, F32 dtu) {
    unused(dtu);

    // WARN: dt can be overriden to dtu! This happens when we are in ingame debug mode.

    p->in_frustum = c3d_is_obb_in_frustum(p->obb);

    // Apply terrain gravity unless noclip is on, manual vertical movement is happening, or standing on triangle mesh floor
    if (!c_debug__noclip && !p->on_triangle_floor && !is_down(IA_MOVE_3D_UP) && !is_down(IA_MOVE_3D_DOWN)) {
        math_keep_player_on_ground(world->base_terrain, dt, p);
    }

}

void player_activate_camera(Player *p) {
    c3d_set(&p->camera3d);
}

void player_input_update(Player *p, F32 dt, F32 dtu) {
    unused(dt);

    BOOL const is_not_overlay = g_scenes.current_overlay_scene_type == SCENE_NONE;
    if (is_not_overlay && !c_console__enabled && is_pressed(IA_TOGGLE_OVERWORLD_AND_DUNGEON_SCENE)) {
        SceneType const current = g_scenes.current_scene_type;
        scenes_set_scene(current == SCENE_OVERWORLD ? SCENE_DUNGEON : SCENE_OVERWORLD );
        audio_play(ACG_SFX, "mario_camera_moving.ogg");
    }

    input_camera_input_update(dtu, &p->camera3d, c_debug__noclip ? c_debug__noclip_move_speed : PLAYER_MOVE_SPEED);
}

void player_draw_3d_hud(Player *p) {}

void player_draw_3d_dbg(Player *p) {
    // TODO: Calculate rotation.

    Vector3 const scale    = {1.1F, 1.1F, 1.1F};
    Vector3 dummy_position = p->camera3d.position;
    dummy_position.y      -= PLAYER_HEIGHT_TO_EYES;

    Camera const active_camera = c3d_get();

    F32 const size = glm::max(Vector3Distance(active_camera.position, dummy_position) * 0.03F, 0.3F);
    d3d_sphere(dummy_position, size * 0.75F, ColorAlpha(WHITE, 0.75F));
    Vector3 const icon_position = Vector3Add(dummy_position, Vector3Scale(Vector3Normalize(Vector3Subtract(active_camera.position, dummy_position)), size));
    d3d_billboard(p->debug_icon, icon_position, size, WHITE);
}

void player_step(Player *p) {
    unused(p);

    // Play footstep sound
    FMOD::Channel *channel = audio_play_with_channel(ACG_SFX, "step.ogg");
    if (channel) {
        BOOL static left_foot = true;
        channel->setPitch(random_f32(0.95F, 1.05F));
        channel->setVolume(random_f32(0.9F, 1.0F));
        channel->setPan(left_foot ? -0.3F : 0.3F);
        left_foot = !left_foot;

        // Add subtle reverb for dungeon atmosphere
        FMOD::DSP *reverb_dsp = nullptr;
        if (audio_get_fmod_system()->createDSPByType(FMOD_DSP_TYPE_SFXREVERB, &reverb_dsp) == FMOD_OK) {
            // Make reverb more subtle
            reverb_dsp->setParameterFloat(FMOD_DSP_SFXREVERB_WETLEVEL, -8.0F); // Lower wet signal (default is -6)
            reverb_dsp->setParameterFloat(FMOD_DSP_SFXREVERB_DRYLEVEL, 0.0F);   // Keep dry signal at full
            reverb_dsp->setParameterFloat(FMOD_DSP_SFXREVERB_DECAYTIME, 1.2F);  // Shorter decay time
            channel->addDSP(0, reverb_dsp);
        }
    }
}
