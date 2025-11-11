#include "player.hpp"
#include "asset.hpp"
#include "audio.hpp"
#include "cvar.hpp"
#include "input.hpp"
#include "math.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "scene_constants.hpp"
#include "world.hpp"

#include <raymath.h>
#include <glm/gtc/type_ptr.hpp>

Player g_player = {};

void player_init() {
    // g_player.cameras[SCENE_OVERWORLD].fovy       = 80.0F;
    // g_player.cameras[SCENE_OVERWORLD].position   = PLAYER_POSITION;
    // g_player.cameras[SCENE_OVERWORLD].projection = CAMERA_PERSPECTIVE;
    // g_player.cameras[SCENE_OVERWORLD].target     = PLAYER_LOOK_AT;
    // g_player.cameras[SCENE_OVERWORLD].up         = {0.0F, 1.0F, 0.0F};

    g_player.cameras[SCENE_OVERWORLD].fovy       = 60.0F;
    g_player.cameras[SCENE_OVERWORLD].position   = {434.4F, 215.0F, 350.0F};
    g_player.cameras[SCENE_OVERWORLD].projection = CAMERA_ORTHOGRAPHIC;
    g_player.cameras[SCENE_OVERWORLD].target     = {436.0F, 211.8F, 352.7F};
    g_player.cameras[SCENE_OVERWORLD].up         = {0.0F, 1.0F, 0.0F};

    g_player.cameras[SCENE_DUNGEON].fovy       = 80.0F;
    g_player.cameras[SCENE_DUNGEON].position   = PLAYER_POSITION;
    g_player.cameras[SCENE_DUNGEON].projection = CAMERA_PERSPECTIVE;
    g_player.cameras[SCENE_DUNGEON].target     = PLAYER_LOOK_AT;
    g_player.cameras[SCENE_DUNGEON].up         = {0.0F, 1.0F, 0.0F};

    g_player.cameras[SCENE_COLLISION_TEST].fovy       = 40.0F;
    g_player.cameras[SCENE_COLLISION_TEST].position   = {434.4F, 215.0F, 350.0F};
    g_player.cameras[SCENE_COLLISION_TEST].projection = CAMERA_ORTHOGRAPHIC;
    g_player.cameras[SCENE_COLLISION_TEST].target     = {436.0F, 211.8F, 352.7F};
    g_player.cameras[SCENE_COLLISION_TEST].up         = {0.0F, 1.0F, 0.0F};

    c3d_copy_from_other(&g_render.cameras.c3d.default_cam, &g_player.cameras[SCENE_OVERWORLD]);

    g_player.debug_icon = asset_get_texture("you_are_here.png");
}

void player_update(F32 dt, F32 dtu) {
    unused(dtu);

    // WARN: dt can be overriden to dtu! This happens when we are in ingame debug mode.

    g_player.in_frustum = c3d_is_obb_in_frustum(g_player.obb);

    // Apply terrain gravity unless noclip is on, manual vertical movement is happening, or standing on triangle mesh floor
    if (!c_debug__noclip && !g_player.on_triangle_floor && !is_down(IA_MOVE_3D_UP) && !is_down(IA_MOVE_3D_DOWN)) {
        math_keep_player_on_ground(g_world->base_terrain, dt);
    }
}


void player_input_update(F32 dt, F32 dtu) {
    unused(dt);

    BOOL const is_not_overlay = g_scenes.current_overlay_scene_type == SCENE_NONE;
    if (is_not_overlay && !c_console__enabled && is_pressed(IA_TOGGLE_OVERWORLD_AND_DUNGEON_SCENE)) {
        SceneType const current = g_scenes.current_scene_type;
        scenes_set_scene(current == SCENE_OVERWORLD ? SCENE_DUNGEON : SCENE_OVERWORLD );
        audio_play(ACG_SFX, "mario_camera_moving.ogg");
    }

    input_camera_input_update(dtu, c3d_get_ptr(), c_debug__noclip ? c_debug__noclip_move_speed : PLAYER_MOVE_SPEED);
}

void player_set_camera(SceneType type) {
    c3d_set(&g_player.cameras[type]);
}

void player_draw_3d_hud() {}

void player_draw_3d_dbg() {
    // TODO: Calculate rotation.

    Vector3 const scale    = {1.1F, 1.1F, 1.1F};
    Vector3 dummy_position = g_player.cameras[g_scenes.current_scene_type].position;
    dummy_position.y      -= PLAYER_HEIGHT_TO_EYES;

    Camera const active_camera = c3d_get();

    F32 const size = glm::max(Vector3Distance(active_camera.position, dummy_position) * 0.03F, 0.3F);
    d3d_sphere(dummy_position, size * 0.75F, ColorAlpha(WHITE, 0.75F));
    Vector3 const icon_position = Vector3Add(dummy_position, Vector3Scale(Vector3Normalize(Vector3Subtract(active_camera.position, dummy_position)), size));
    d3d_billboard(g_player.debug_icon, icon_position, size, WHITE);
}

void player_step() {
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

void player_dump_cameras() {
    // Find the longest scene name for proper column alignment
    SZ max_scene_name_len = 0;
    for (SZ i = 0; i < SCENE_COUNT; ++i) {
        SZ const len = strlen(scenes_to_cstr((SceneType)i));
        if (len > max_scene_name_len) { max_scene_name_len = len; }
    }

    // Print header
    lli("%-*s  %8s %8s %8s  %8s", (int)max_scene_name_len, "Scene", "Pos X", "Pos Y", "Pos Z", "FOV");
    lli("%-*s  %8s %8s %8s  %8s", (int)max_scene_name_len, "-----", "-----", "-----", "-----", "---");

    // Print each camera's data
    for (SZ i = 0; i < SCENE_COUNT; ++i) {
        lli("%-*s  %8.2f %8.2f %8.2f  %8.2f",
            (int)max_scene_name_len,
            scenes_to_cstr((SceneType)i),
            g_player.cameras[i].position.x,
            g_player.cameras[i].position.y,
            g_player.cameras[i].position.z,
            g_player.cameras[i].fovy
        );
    }
}
