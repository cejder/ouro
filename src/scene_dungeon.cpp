#include "asset.hpp"
#include "audio.hpp"
#include "color.hpp"
#include "common.hpp"
#include "console.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "dungeon.hpp"
#include "ease.hpp"
#include "entity.hpp"
#include "fade.hpp"
#include "fog.hpp"
#include "grid.hpp"
#include "hud.hpp"
#include "input.hpp"
#include "light.hpp"
#include "math.hpp"
#include "menu.hpp"
#include "message.hpp"
#include "particles_3d.hpp"
#include "player.hpp"
#include "profiler.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "selection_indicators.hpp"
#include "scene_constants.hpp"
#include "string.hpp"
#include "talk.hpp"
#include "word.hpp"
#include "world.hpp"

#include <raymath.h>

struct State {
    Menu menu;
    Scene *scene;
} static s = {};

void static i_toggle_fn(void *data) {
    auto *toggle_data = (MenuExtraToggleData *)data;
    *toggle_data->toggle = !*toggle_data->toggle;
    mio(TS("%s %s", toggle_data->name, *toggle_data->toggle ? "\\ouc{#00ff00ff}enabled" : "\\ouc{#ff0000ff}disabled")->c, WHITE);
}

void static i_toggle_camera(void *data) {
    unused(data);

    Camera3D const *camera = c3d_get_ptr();
    if (camera == &g_player.cameras[SCENE_DUNGEON]) {
        c3d_set(c3d_get_default_ptr());
        mio("Switching to \\ouc{#ffff00ff}default camera", WHITE);
        g_player.non_player_camera = true;
    } else {
        c3d_set(&g_player.cameras[SCENE_DUNGEON]);
        mio("Switching to \\ouc{#00ff00ff}player camera", WHITE);
        g_player.non_player_camera = false;
    }
}

SCENE_INIT(dungeon) {
    s.scene = scene;

    menu_init_with_blackbox_menu_defaults(&s.menu,
                                          menu_extra_dungeon_func_get_entry_count(),
                                          menu_extra_dungeon_func_get_label,
                                          nullptr,
                                          menu_extra_dungeon_func_get_desc,
                                          nullptr,
                                          menu_extra_dungeon_func_get_value,
                                          nullptr,
                                          menu_extra_dungeon_func_yes,
                                          menu_extra_dungeon_func_change,
                                          (void *)(&s.menu.selected_entry));
    s.menu.enabled = false;

    world_set_dungeon(g_world->base_terrain = asset_get_terrain("flat", {(F32)A_TERRAIN_DEFAULT_SIZE, (F32)A_TERRAIN_DEFAULT_SIZE, (F32)A_TERRAIN_DEFAULT_SIZE}));

    entity_create(ENTITY_TYPE_PROP, "TORCH_LEFT", {502.50F, 5.5F, 776.20F}, 0.0F, {10.0F, 10.0F, 10.0F}, GOLD, "torch.glb");
    entity_create(ENTITY_TYPE_PROP, "TORCH_RIGHT", {517.50F, 5.5F, 776.20F}, 0.0F, {10.0F, 10.0F, 10.0F}, GOLD, "torch.glb");

    for (SZ i = 0; i < 32; ++i) {
        for (SZ j = 0; j < 32; ++j) {
          Vector3 pos = {
              200.00F - ((F32)j * (5.0F * math_cos_f32((F32)j))),
              5.5F,
              200.00F - ((F32)i * (5.0F * math_sin_f32((F32)i))),
          };
          EID const e = entity_create(ENTITY_TYPE_NPC, word_generate_name()->c, pos, 0.0F, {1.5F, 1.5F, 1.5F}, color_random_vibrant(), "greenman.glb");
          pos.y = math_get_terrain_height(g_world->base_terrain, pos.x, pos.z);
          entity_set_position(e, pos);
          entity_enable_actor(e);
        }
    }
}

SCENE_ENTER(dungeon) {
    world_set_dungeon(g_world->base_terrain = asset_get_terrain("flat", {(F32)A_TERRAIN_DEFAULT_SIZE, (F32)A_TERRAIN_DEFAULT_SIZE, (F32)A_TERRAIN_DEFAULT_SIZE}));

    s.scene->clear_color = BLACK;

    // hud_init(5.0F, 12.5F, 5.0F, 5.0F);
    hud_init(0.0F, 0.0F, 0.0F, 0.0F);
    player_set_camera(SCENE_DUNGEON);

    lighting_default_lights_setup();

    audio_play(ACG_MUSIC, "drone.ogg");

    g_player.non_player_camera = false;

    screen_fade_init(SCREEN_FADE_TYPE_FADE_OUT, SCREEN_FADE_DEFAULT_DURATION, g_render.accent_color, EASE_IN_OUT_SINE, nullptr);
}

SCENE_RESUME(dungeon) {
    audio_resume(ACG_MUSIC);
    screen_fade_init(SCREEN_FADE_TYPE_FADE_OUT, SCREEN_FADE_DEFAULT_DURATION, g_render.accent_color, EASE_IN_OUT_SINE, nullptr);
}

SCENE_EXIT(dungeon) {
    audio_reset_all();
}

SCENE_UPDATE(dungeon) {
    if (c_console__enabled) { goto SKIP_OTHER_INPUT;  } // Early skip for console

    if (is_pressed(IA_DBG_RESET_CAMERA))                   { player_init();                                                                               }
    if (is_pressed(IA_DBG_TOGGLE_NOCLIP))                  { MenuExtraToggleData toggle_data = { &c_debug__noclip, "Noclip" }; i_toggle_fn(&toggle_data); }
    if (is_pressed(IA_DBG_TOGGLE_CAMERA))                  { i_toggle_camera(nullptr);                                                                    }
    if (is_pressed(IA_DBG_PULL_DEFAULT_CAM_TO_PLAYER_CAM)) { c3d_pull_default_to_other(&g_player.cameras[SCENE_DUNGEON]);                                        }
    if (is_pressed(IA_OPEN_OVERLAY_MENU))                  { scenes_push_overlay_scene(SCENE_OVERLAY_MENU);                                               }
    if (is_pressed(IA_DBG_OPEN_TEST_MENU))                 { s.menu.enabled = !s.menu.enabled;                                                            }

    if (s.menu.enabled && is_pressed(IA_NO)) { s.menu.enabled = false; goto SKIP_OTHER_INPUT; } // Early skip for when menu is closed.

    // Menu and lighting input/update
    PP(menu_update(&s.menu));
    PP(input_lighting_input_update());

SKIP_OTHER_INPUT:
    // Camera update
    F32 dt_for_debug = dt;
    if (c_debug__enabled) { dt_for_debug = dtu; }

    if (g_player.non_player_camera) {
        PP(input_camera_input_update(dt_for_debug, c3d_get_ptr(), c_debug__noclip_move_speed));
    } else {
        PP(player_input_update(dt_for_debug, dtu));
    }

    // Early return if scene changed (e.g., tab to switch scenes)
    if (g_scenes.next_scene_type != SCENE_NONE) { return; }

    // World state recording controls
    if (is_pressed(IA_DBG_WORLD_STATE_RECORD))   { world_recorder_toggle_record_state(); }
    if (is_pressed(IA_DBG_WORLD_STATE_PLAY))     { world_recorder_toggle_loop_state();   }
    if (is_down(IA_DBG_WORLD_STATE_BACKWARD))    { world_recorder_backward_state();      }
    if (is_down(IA_DBG_WORLD_STATE_FORWARD))     { world_recorder_forward_state();       }

    // End-of-frame updates
    g_fog.density = c_render__dungeon_fog_density;
    PP(screen_fade_update(dt));

    PP(player_update(dt, dtu));
    PP(dungeon_update(dt));
    PP(world_update(dt, dtu));
    PP(hud_update(dt, dtu));

    Vector3 pos = c3d_get_ptr()->position;
    pos.y += PLAYER_HEIGHT_TO_EYES * 2; // Slightly above the player
    lighting_set_point_light(0, true, pos, WHITE, 1000.0F); // TODO: REMOVE ME

    lighting_set_point_light(1, true, {502.50F, 6.5F, 780.00F}, color_sync_blinking_kinda_fast(ORANGE, PERSIMMON), 75.0F);
    lighting_set_point_light(2, true, {517.50F, 6.5F, 780.00F}, color_sync_blinking_kinda_fast(TANGERINE, TUSCAN), 75.0F);

    // Spawn fire and smoke particles for the torches (frame-rate independent)
    F32 static particle_timer = 0.0F;
    particle_timer += dt;

    F32 const particles_per_second = 50.0F;
    F32 const spawn_interval = 1.0F / particles_per_second;
    if (particle_timer >= spawn_interval) {
        particle_timer -= spawn_interval;

        Vector3 const torch_left_pos  = {502.50F, 8.5F, 776.80F};
        Vector3 const torch_right_pos = {517.50F, 8.5F, 776.80F};

        particles3d_add_fire(torch_left_pos, 0.3F, YELLOW, RED, 0.3F, 1);
        particles3d_add_smoke(torch_left_pos, 0.2F, ORANGE, RED, 0.25F, 1);

        particles3d_add_fire(torch_right_pos, 0.3F, YELLOW, RED, 0.3F, 1);
        particles3d_add_smoke(torch_right_pos, 0.2F, ORANGE, RED, 0.25F, 1);
    }
}

SCENE_DRAW(dungeon) {
    Vector2 const res = render_get_render_resolution();;
    Camera3D *other_camera = g_player.non_player_camera ? &g_player.cameras[SCENE_DUNGEON] : c3d_get_default_ptr();

    render_set_render_mode_order_for_frame((RenderModeOrderTarget){
        RMODE_3D_SKETCH,
        RMODE_3D,
        RMODE_3D_HUD_SKETCH,
        RMODE_3D_HUD,
        RMODE_3D_DBG,

        RMODE_2D_SKETCH,
        RMODE_2D,
        RMODE_2D_HUD_SKETCH,
        RMODE_2D_HUD,
        RMODE_2D_DBG,

        RMODE_LAST_LAYER,
    });

    RMODE_BEGIN(RMODE_3D_SKETCH) {
        if (c_render__skybox) { d3d_skybox_active(); }

        d3d_terrain_ex(asset_get_terrain("flat",{(F32)A_TERRAIN_DEFAULT_SIZE, (F32)A_TERRAIN_DEFAULT_SIZE, (F32)A_TERRAIN_DEFAULT_SIZE}),
                       {}, {A_TERRAIN_DEFAULT_SCALE, A_TERRAIN_DEFAULT_SCALE, A_TERRAIN_DEFAULT_SCALE}, WHITE);


        dungeon_draw_3d_sketch();
        world_draw_3d_sketch();
        particles3d_draw();
        selection_indicators_draw();
    }
    RMODE_END;

    RMODE_BEGIN(RMODE_3D) {
        if (!g_player.non_player_camera) { player_draw_3d_hud(); }

        world_draw_3d();
    }
    RMODE_END;

    RMODE_BEGIN(RMODE_3D_HUD_SKETCH) {
        if (!g_player.non_player_camera) { player_draw_3d_hud(); }

        world_draw_3d_hud();
    }
    RMODE_END;

    RMODE_BEGIN(RMODE_3D_DBG) {
        if (c_debug__enabled) {
            if (g_player.non_player_camera) { player_draw_3d_dbg(); }
            world_draw_3d_dbg();
            lighting_draw_3d_dbg();
            render_draw_3d_dbg(other_camera);
            audio_draw_3d_dbg();
        }
    }
    RMODE_END;

    RMODE_BEGIN(RMODE_2D) {
        world_draw_2d();
    }
    RMODE_END;

    RMODE_BEGIN(RMODE_2D_HUD) {
        world_draw_2d_hud();

        if (g_player.non_player_camera) {
            d2d_rectangle_v({}, res, Fade(MAGENTA, 0.25F));
            C8 const *text = "NON-PLAYER CAMERA ACTIVE";
            AFont *font = asset_get_font("spleen-8x16", 16);
            Vector2 const text_size = measure_text(font, text);
            Vector2 const center_position = {(res.x / 2.0F) - (text_size.x / 2.0F), (((F32)font->font_size + text_size.y) / 2.0F) + ui_scale_y(15.0F)};
            d2d_rectangle_v(center_position, text_size, Fade(BLACK, 0.85F));
            d2d_text(font, text, center_position, SALMON);
        }

        screen_fade_draw_2d_hud();
        hud_draw_2d_hud();
        menu_draw_2d_hud(&s.menu);
    }
    RMODE_END;

    RMODE_BEGIN(RMODE_2D_HUD_SKETCH) {
        hud_draw_2d_hud_sketch();
    }
    RMODE_END;

    RMODE_BEGIN(RMODE_2D_DBG) {
        if (c_debug__enabled) {
            world_draw_2d_dbg();
            dungeon_draw_2d_dbg();
            lighting_draw_2d_dbg();
            render_draw_2d_dbg(other_camera);
        }

        menu_draw_2d_dbg(&s.menu);
    }
    RMODE_END;
}
