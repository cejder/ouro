#include "asset.hpp"
#include "audio.hpp"
#include "console.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "debug_quick.hpp"
#include "ease.hpp"
#include "entity_spawn.hpp"
#include "fade.hpp"
#include "fog.hpp"
#include "grid.hpp"
#include "hud.hpp"
#include "input.hpp"
#include "light.hpp"
#include "math.hpp"
#include "menu.hpp"
#include "message.hpp"
#include "particles_2d.hpp"
#include "particles_3d.hpp"
#include "profiler.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "scene_constants.hpp"
#include "string.hpp"
#include "talk.hpp"
#include "time.hpp"
#include "world.hpp"

#include <raymath.h>
#include <stdlib.h>
#include <glm/gtc/type_ptr.hpp>

struct State {
    struct {
        BOOL enabled;
        F32 duration;
    } fade_out;

    EntityTestOverworldSet entities;
    Menu menu;
    Scene *scene;

    Mesh manual_mesh;

    // C8 const *sponza_model_name;
    // EID sponza_entity;

    F32 ambient_particles_per_second;
    F32 ambient_particles_scale;
    ColorEditState particle_start_color_state;
    ColorEditState particle_end_color_state;
} static s = {};

Mesh static i_generate_manual_mesh() {
    F32 const size = 1.0F;
    Mesh mesh      = GenMeshCube(size, size, size);
    UploadMesh(&mesh, false);
    return mesh;
}

void static i_reset_cameras(void *data) {
    BOOL const during_enter = (BOOL)(data);

    Camera3D *c = &g_world->player.camera3d;
    c->position   = {434.4F, 215.0F, 350.0F};
    c->target     = {436.0F, 211.8F, 352.7F};
    c->up         = {0.0F, 1.0F, 0.0F};
    c->fovy       = 40.0F;
    c->projection = CAMERA_ORTHOGRAPHIC;

    c = c3d_get_default_ptr();
    c->position   = PLAYER_POSITION;
    c->target     = PLAYER_LOOK_AT;
    c->up         = {0.0F, 1.0F, 0.0F};
    c->fovy       = 80.0F;
    c->projection = CAMERA_PERSPECTIVE;

    if (!during_enter) { mi("Cameras reset", WHITE); }
}

void static i_toggle_fn(void *data) {
    auto *toggle_data = (MenuExtraToggleData *)data;
    *toggle_data->toggle = !*toggle_data->toggle;
    mio(TS("%s %s", toggle_data->name, *toggle_data->toggle ? "\\ouc{#00ff00ff}enabled" : "\\ouc{#ff0000ff}disabled")->c, WHITE);
}

void static i_toggle_camera(void *data) {
    unused(data);

    Camera3D const *camera = c3d_get_ptr();
    if (camera == &g_world->player.camera3d) {
        c3d_set(c3d_get_default_ptr());
        mio("Switching to \\ouc{#ffff00ff}default camera", WHITE);
        g_world->player.non_player_camera = true;
    } else {
        c3d_set(&g_world->player.camera3d);
        mio("Switching to \\ouc{#00ff00ff}player camera", WHITE);
        g_world->player.non_player_camera = false;
    }
}

void static i_reset_lights(void *data) {
    unused(data);

    lighting_default_lights_setup();
}

void static i_harvest_trees(void *data) {
    unused(data);

    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (g_world->type[i] != ENTITY_TYPE_NPC) { continue; }
        entity_actor_start_looking_for_target(i, ENTITY_TYPE_VEGETATION);
    }
}

void static i_gather_at_prop(void *data) {
    unused(data);

    // Find first prop to gather at
    EID prop_target = INVALID_EID;
    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (g_world->type[i] == ENTITY_TYPE_PROP) {
            prop_target = i;
            break;
        }
    }

    if (prop_target == INVALID_EID) { return; }

    // Command all NPCs to move near the prop from their current position
    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (g_world->type[i] != ENTITY_TYPE_NPC) { continue; }
        entity_actor_start_looking_for_target(i, ENTITY_TYPE_PROP);
    }
}

void static i_just_chill(void *data) {
    unused(data);

    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (g_world->type[i] != ENTITY_TYPE_NPC) { continue; }
        entity_actor_behavior_transition_to_state(i, ENTITY_BEHAVIOR_STATE_IDLE, "was told to chill");
    }
}

void static i_attack_other_close_npc(void *data) {
    unused(data);

    // First, collect all NPCs
    EID npcs[WORLD_MAX_ENTITIES];
    SZ npc_count = 0;

    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (g_world->type[i] != ENTITY_TYPE_NPC) { continue; }
        npcs[npc_count++] = i;
    }

    // Need at least 2 NPCs to have attacking
    if (npc_count < 2) { return; }

    // Make each NPC attack their closest enemy
    for (SZ i = 0; i < npc_count; ++i) {
        EID const attacker = npcs[i];
        Vector3 const attacker_pos = g_world->position[attacker];

        EID closest_enemy = INVALID_EID;
        F32 closest_distance = F32_MAX;

        // Find the closest other NPC
        for (SZ j = 0; j < npc_count; ++j) {
            if (i == j) { continue; }  // Skip self

            EID const potential_victim = npcs[j];
            Vector3 const victim_pos   = g_world->position[potential_victim];
            F32 const distance         = Vector3Distance(attacker_pos, victim_pos);

            if (distance < closest_distance) {
                closest_distance = distance;
                closest_enemy = potential_victim;
            }
        }

        // Attack the closest enemy
        if (closest_enemy != INVALID_EID) { entity_actor_set_attack_target_npc(attacker, closest_enemy); }
    }
}

void static i_test_3d_audio_stop(void *data) {
    unused(data);

    audio_stop_all();
}

void static i_test_3d_audio_close(void *data) {
    unused(data);

    // Play a sound close to the player (should be loud)
    Camera3D const camera   = c3d_get();
    Vector3 const close_pos = {camera.position.x + 5.0F, camera.position.y, camera.position.z};  // 5 units to the right

    FMOD::Channel const *channel = audio_play_3d_at_position(ACG_MUSIC, "hanna_blues.ogg", close_pos);
    if (channel) { mi("Playing 3D audio close to player", WHITE); }
}

void static i_test_3d_audio_far(void *data) {
    unused(data);

    // Play a sound far from the player (should be quiet/silent)
    Camera3D const camera = c3d_get();
    Vector3 const far_pos = {camera.position.x + 200.0F,  // 200 units away
                             camera.position.y, camera.position.z};

    FMOD::Channel const *channel = audio_play_3d_at_position(ACG_MUSIC, "haft.ogg", far_pos);
    if (channel) { mi("Playing 3D audio far from player", WHITE); }
}

void static i_test_3d_audio_behind(void *data) {
    unused(data);

    // Play a sound behind the player (should be panned to back)
    Camera3D const camera    = c3d_get();
    Vector3 const forward    = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 const behind_pos = {camera.position.x - (forward.x * 20.0F), camera.position.y, camera.position.z - (forward.z * 20.0F)};

    FMOD::Channel const *channel = audio_play_3d_at_position(ACG_MUSIC, "haft.ogg", behind_pos);
    if (channel) { mi("Playing 3D audio behind player", WHITE); }
}

void static i_test_3d_audio_moving(void *data) {
    unused(data);

    // Find a random NPC and play a sound at its position
    BOOL found_npc = false;
    for (SZ idx = 0; idx < g_world->active_entity_count && !found_npc; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (g_world->type[i] != ENTITY_TYPE_NPC) { continue; }

        FMOD::Channel const *channel = audio_play_3d_at_entity(ACG_MUSIC, "haft.ogg", i);
        if (channel) {
            mi(TS("Playing 3D audio at NPC entity %u", i)->c, WHITE);
            found_npc = true;
        }
    }

    if (!found_npc) { mi("No NPCs found to attach audio to", YELLOW); }
}

void static i_test_3d_audio_corners(void *data) {
    unused(data);

    // Play sounds at all four corners of the world (1024x1024)
    F32 height = 150.0F;
    Vector3 const corners[4] = {
        {0.0F, height, 0.0F},                                     // Bottom-left
        {A_TERRAIN_DEFAULT_SIZE, height, 0.0F},                   // Bottom-right
        {0.0F, height, A_TERRAIN_DEFAULT_SIZE},                   // Top-left
        {A_TERRAIN_DEFAULT_SIZE, height, A_TERRAIN_DEFAULT_SIZE}  // Top-right
    };

    for (S32 i = 0; i < 4; ++i) {
        FMOD::Channel const *channel = audio_play_3d_at_position(ACG_MUSIC, "haft.ogg", corners[i]);
        if (channel) { mi(TS("Playing 3D audio at corner %d: (%.0f, %.0f, %.0f)", i + 1, corners[i].x, corners[i].y, corners[i].z)->c, WHITE); }
    }
}

void static i_toggle_3d_doppler(void *data) {
    unused(data);

    audio_3d_set_doppler_enabled(!c_audio__doppler_enabled);

    mi(TS("3D Audio Doppler %s", !c_audio__doppler_enabled ? "enabled" : "disabled")->c, !c_audio__doppler_enabled ? GREEN : RED);
}

void static i_cycle_3d_rolloff(void *data) {
    unused(data);

    Audio3DRolloff const current_rolloff = g_audio.rolloff_type;
    auto new_rolloff = (Audio3DRolloff)((current_rolloff + 1) % AUDIO_3D_ROLLOFF_COUNT);
    audio_3d_set_rolloff_type(new_rolloff);

    C8 const *rolloff_names[] = {"Inverse", "Linear", "Linear Square", "Inverse Tapered", "Custom"};

    mi(TS("3D Audio Rolloff: %s", rolloff_names[new_rolloff])->c, CYAN);
}

void static i_dbg_ui_cb(void *data) {
    unused(data);

    AFont *font_s = asset_get_font(c_debug__small_font.cstr, c_debug__small_font_size);
    AFont *font_m = asset_get_font(c_debug__medium_font.cstr, c_debug__medium_font_size);

    // Particles
    dwilo("Particles", font_m, GOLD);
    dwis(5.0F);

    dwifb("Rate", font_s, DBG_FILLBAR_FG_COLOR, DBG_FILLBAR_BG_COLOR, DBG_FILLBAR_TEXT_COLOR, DBG_REF_FILLBAR_SIZE, s.ambient_particles_per_second, 0.0F, 10000.0F, true, &s.ambient_particles_per_second, DBG_WIDGET_DATA_TYPE_FLOAT, DBG_WIDGET_CB_NONE);
    dwifb("Scale", font_s, DBG_FILLBAR_FG_COLOR, DBG_FILLBAR_BG_COLOR, DBG_FILLBAR_TEXT_COLOR, DBG_REF_FILLBAR_SIZE, s.ambient_particles_scale, 1.0F, 100.0F, true, &s.ambient_particles_scale, DBG_WIDGET_DATA_TYPE_FLOAT, DBG_WIDGET_CB_NONE);

    dwis(10.0F);

    dbg_quick_color_edit("Start Color", &s.particle_start_color_state.color, &s.particle_start_color_state, DBG_WIDGET_CB_NONE);
    dwis(25.0F);
    dbg_quick_color_edit("End Color", &s.particle_end_color_state.color, &s.particle_end_color_state, DBG_WIDGET_CB_NONE);

    // 3D Audio
    dwis(10.0F);
    dwilo("3D Audio Tests", font_m, GOLD);
    dwis(5.0F);

    dwib("Stop", font_s, RED, DBG_REF_BUTTON_SIZE, i_test_3d_audio_stop, nullptr);
    dwib("Play Close (5m)", font_s, GREEN, DBG_REF_BUTTON_SIZE, i_test_3d_audio_close, nullptr);
    dwib("Play Far (200m)", font_s, ORANGE, DBG_REF_BUTTON_SIZE, i_test_3d_audio_far, nullptr);
    dwib("Play Behind", font_s, PURPLE, DBG_REF_BUTTON_SIZE, i_test_3d_audio_behind, nullptr);
    dwib("Play at NPC", font_s, CYAN, DBG_REF_BUTTON_SIZE, i_test_3d_audio_moving, nullptr);
    dwib("Play at Corners", font_s, ORANGE, DBG_REF_BUTTON_SIZE, i_test_3d_audio_corners, nullptr);

    dwis(5.0F);
    dwilo("3D Audio Settings", font_s, GOLD);
    dwis(5.0F);

    dwib("Toggle Doppler", font_s, SKYBLUE, DBG_REF_BUTTON_SIZE, i_toggle_3d_doppler, nullptr);
    dwib("Cycle Rolloff", font_s, PINK, DBG_REF_BUTTON_SIZE, i_cycle_3d_rolloff, nullptr);

    dwis(5.0F);
    dwilo(TS("Min Distance: %.1f", c_audio__min_distance)->c, font_s, WHITE);
    dwilo(TS("Max Distance: %.1f", c_audio__max_distance)->c, font_s, WHITE);
    dwilo(TS("Doppler: %s", c_audio__doppler_enabled ? "ON" : "OFF")->c, font_s, c_audio__doppler_enabled ? GREEN : RED);

    C8 const *rolloff_names[] = {"Inverse", "Linear", "Linear Square", "Inverse Tapered", "Custom"};
    dwilo(TS("Rolloff: %s", rolloff_names[g_audio.rolloff_type])->c, font_s, WHITE);

    // Misc
    dwis(10.0F);
    dwilo("Misc", font_m, GOLD);
    dwis(5.0F);

    dwis(25.0F);
    dwib("Reset Camera", font_s, NAYBEIGE, DBG_REF_BUTTON_SIZE, i_reset_cameras, (void *)false);
    dwib("Reset Lights", font_s, NAYBEIGE, DBG_REF_BUTTON_SIZE, i_reset_lights, nullptr);
    dwib("Harvest Trees", font_s, NAYBEIGE, DBG_REF_BUTTON_SIZE, i_harvest_trees, (void *)false);
    dwib("Gather at Prop", font_s, NAYBEIGE, DBG_REF_BUTTON_SIZE, i_gather_at_prop, (void *)false);
    dwib("Just chill", font_s, NAYBEIGE, DBG_REF_BUTTON_SIZE, i_just_chill, (void *)false);
    dwib("Attack other NPC", font_s, NAYBEIGE, DBG_REF_BUTTON_SIZE, i_attack_other_close_npc, (void *)false);


    dwis(5.0F);
    SZ static const default_trip = c_render__tboy ? 2 : c_render__fboy ? 1 : 0;
    SZ static selected_trip      = SZ_MAX;
    SZ const trip_type_count     = 3;

    struct ITripEntry {
        BOOL fboy_state;
        BOOL tboy_state;
        C8 const *label;
    } static const trip_array[trip_type_count] = {
        {false, false, "None"},
        {true, false, "FBoy"},
        {false, true, "TBoy"},
    };

    C8 const **trip_types = mmta(C8 const **, sizeof(C8 *) * trip_type_count);
    for (SZ i = 0; i < trip_type_count; ++i) { trip_types[i] = trip_array[i].label; }
    if (selected_trip == DBG_WIDGET_DROPDOWN_ITEM_IDX_NONE) { selected_trip = default_trip; }

    dwidd("Trip", font_s, NAYBEIGE, GOLD, GREEN, DBG_REF_DROPDOWN_SIZE, trip_types, trip_type_count, &selected_trip, nullptr, nullptr);

    const ITripEntry *trip_entry = &trip_array[selected_trip];
    c_render__fboy               = trip_entry->fboy_state;
    c_render__tboy               = trip_entry->tboy_state;
}

void static cb_trigger_gong(void *data) {
    unused(data);

    audio_reset(ACG_SFX);
    audio_play(ACG_SFX, "gong.ogg");
}

void static cb_trigger_end(void *data) {
    unused(data);

    audio_reset(ACG_SFX);
    audio_play(ACG_SFX, "gong2.ogg");
    s.fade_out.enabled = true;
}

void static i_fade_out_maybe(F32 dt) {
    if (s.fade_out.enabled) {
        s.fade_out.duration += dt / 4.0F;

        if (s.fade_out.duration >= 1.0F) {
            s.fade_out.duration = 1.0F;

            scenes_set_scene(SCENE_MENU);
        }
    }
}

SCENE_INIT(collision_test) {
    s.scene = scene;

    menu_init_with_blackbox_menu_defaults(&s.menu,
                                          menu_extra_overworld_func_get_entry_count(),
                                          menu_extra_overworld_func_get_label,
                                          nullptr,
                                          menu_extra_overworld_func_get_desc,
                                          nullptr,
                                          menu_extra_overworld_func_get_value,
                                          nullptr,
                                          menu_extra_overworld_func_yes,
                                          menu_extra_overworld_func_change,
                                          (void *)(&s.menu.selected_entry));
    s.menu.enabled = false;

    s.ambient_particles_per_second         = 10.0F;
    s.ambient_particles_scale              = 1.0F;
    s.particle_start_color_state.color     = WHITE;
    s.particle_start_color_state.color_ptr = &s.particle_start_color_state.color;
    s.particle_end_color_state.color       = CYAN;
    s.particle_end_color_state.color_ptr   = &s.particle_end_color_state.color;
}

SCENE_ENTER(collision_test) {
    lighting_default_lights_setup();
    world_reset();
    world_set_overworld(asset_get_terrain("terrain2", {(F32)A_TERRAIN_DEFAULT_SIZE, (F32)A_TERRAIN_DEFAULT_SIZE, (F32)A_TERRAIN_DEFAULT_SIZE}));
    entity_spawn_test_overworld_set(&s.entities);
    entity_spawn_random_vegetation_on_terrain(g_world->base_terrain, (SZ)TREE_COUNT * 2, false);
    entity_init_test_overworld_set_talkers(&s.entities, cb_trigger_gong, cb_trigger_end);
    i_harvest_trees(nullptr);

    s.scene->clear_color = BLACK;

    hud_init(10.0F, 12.5F, 5.0F, 5.0F);

    i_reset_lights(nullptr);

    dbg_add_cb(DBG_WID_CUSTOM_0, {i_dbg_ui_cb, nullptr});

    player_init(&g_world->player, PLAYER_POSITION, PLAYER_LOOK_AT);
    player_activate_camera(&g_world->player);

    i_reset_cameras((void *)true);
    c3d_copy_from_other(c3d_get_default_ptr(), &g_world->player.camera3d);

    audio_play(ACG_MUSIC, "bossa.ogg");
    audio_play(ACG_AMBIENCE, "cuckoo.ogg");
    audio_attach_dsp_by_type(ACG_AMBIENCE, FMOD_DSP_TYPE_SFXREVERB);

    g_world->player.non_player_camera = false;

    // Unrelated to s._screen_fade
    s.fade_out.duration = 0.0F;
    s.fade_out.enabled = false;

    screen_fade_init(SCREEN_FADE_TYPE_WIPE_RIGHT_OUT, SCREEN_FADE_DEFAULT_DURATION * 2.0F, g_render.accent_color, EASE_IN_OUT_SINE, nullptr);

    c_debug__noclip = true;

    F32 const dtmod = 2.0F;
    time_set_delta_mod(dtmod);
    mqf("Time Delta Modifier set to %.2f", dtmod);

    entity_spawn_random_npc_around_camera(1000, false);
    // TODO: Draw slider on this screen to adjust delta time mod.
}

SCENE_RESUME(collision_test) {
    audio_resume(ACG_MUSIC);
    screen_fade_init(SCREEN_FADE_TYPE_WIPE_LEFT_OUT, SCREEN_FADE_QUICK_DEFAULT_DURATION * 2.0F, g_render.accent_color, EASE_IN_OUT_SINE, nullptr);
}

SCENE_EXIT(collision_test) {
    dbg_remove_cb(DBG_WID_CUSTOM_0, {i_dbg_ui_cb, nullptr});
    audio_reset_all();
    time_set_delta_mod(1.0F);
}

SCENE_UPDATE(collision_test) {
    if (c_console__enabled) { goto SKIP_OTHER_INPUT;  } // Early skip for console

    if (is_pressed(IA_DBG_RESET_CAMERA))                   { i_reset_cameras((void*)false);                                                               }
    if (is_pressed(IA_DBG_TOGGLE_NOCLIP))                  { MenuExtraToggleData toggle_data = { &c_debug__noclip, "Noclip" }; i_toggle_fn(&toggle_data); }
    if (is_pressed(IA_DBG_TOGGLE_CAMERA))                  { i_toggle_camera(nullptr);                                                                    }
    if (is_pressed(IA_DBG_PULL_DEFAULT_CAM_TO_PLAYER_CAM)) { c3d_pull_default_to_other(&g_world->player.camera3d);                                        }
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

    if (g_world->player.non_player_camera) {
        PP(input_camera_input_update(dt_for_debug, c3d_get_ptr(), c_debug__noclip_move_speed));
    } else {
        PP(player_input_update(&g_world->player, dt_for_debug, dtu));
    }

    // World state recording controls
    if (is_pressed(IA_DBG_WORLD_STATE_RECORD))   { world_recorder_toggle_record_state(); }
    if (is_pressed(IA_DBG_WORLD_STATE_PLAY))     { world_recorder_toggle_loop_state();   }
    if (is_down(IA_DBG_WORLD_STATE_BACKWARD))    { world_recorder_backward_state();      }
    if (is_down(IA_DBG_WORLD_STATE_FORWARD))     { world_recorder_forward_state();       }

    // End-of-frame updates
    g_fog.density = c_render__overworld_fog_density;
    PP(screen_fade_update(dt));
    PP(i_fade_out_maybe(dt));

    // Triangle collision must run BEFORE player_update so the flag is set
    BOOL const on_triangle_floor = world_triangle_mesh_collision();
    g_world->player.on_triangle_floor = on_triangle_floor;

    PP(player_update(&g_world->player, g_world, dt, dtu));
    PP(world_vegetation_collision());
    PP(world_update(dt, dtu));
    PP(hud_update(dt, dtu));

    // Spawn ambient rain particles all over the terrain
    SZ const ambient_count = (SZ)glm::max(0.0F, glm::round(s.ambient_particles_per_second * dt));

    for (SZ i = 0; i < ambient_count; ++i) {
        F32 const size           = (F32)A_TERRAIN_DEFAULT_SIZE;
        F32 const padding        = size * 0.1F;
        F32 const x              = random_f32(-padding, padding + size);
        F32 const z              = random_f32(-padding, padding + size);
        F32 const terrain_height = math_get_terrain_height(g_world->base_terrain, x, z);
        Vector3 const ground_pos = {x, terrain_height, z};
        F32 const fall_height    = 2000.0F; // Max spawn height above ground
        F32 const spread_radius  = 1.0F;

        particles3d_add_effect_ambient_rain(
                                            ground_pos,
                                            spread_radius,
                                            fall_height,
                                            s.particle_start_color_state.color,
                                            s.particle_end_color_state.color,
                                            s.ambient_particles_scale,
                                            1);
    }

    // F32 static time = 0.0F;
    // F32 const max_time = 15.0F;
    // SZ static frames = 0;
    // SZ const current_gen = g_profiler.current_generation;
    // frames++;
    // time += dt;

    // if (current_gen > 60 && time >= max_time) {
    //     lle("%zu frames with %d cells per row", frames, GRID_CELLS_PER_ROW);
    //     exit(EXIT_SUCCESS);
    // }

    // Vector2 const mouse_pos = input_get_mouse_position();

    // if (is_mouse_down(I_MOUSE_LEFT) && !dbg_is_mouse_over_or_dragged()) {
    //     Camera3D *camera                     = c3d_get_ptr();
    //     Ray const ray                        = GetScreenToWorldRay(mouse_pos, *camera);
    //     RayCollision const terrain_collision = math_ray_collision_to_terrain(g_world->base_terrain, ray.position, ray.direction);

    //     if (terrain_collision.hit) {
    //         F32 const particles_per_second = 1000.0F;
    //         SZ const count                 = (SZ)glm::round(particles_per_second * dt);
    //         F32 const scale                = 1.0F;
    //         F32 const vert_offset          = 2.5F;
    //         Vector3 const spawn_pos        = {terrain_collision.point.x, terrain_collision.point.y + vert_offset, terrain_collision.point.z};

    //         if (count > 0) {
    //             particles3d_add_effect_explosion        (spawn_pos, 2.0F, PURPLE, RED, scale, count);
    //             particles3d_add_effect_smoke            (spawn_pos, 1.5F, LIME, WHITE, scale, count);
    //             particles3d_add_effect_sparkle          (spawn_pos, 2.5F, GOLD, GREEN, scale, count);
    //             particles3d_add_effect_fire             (spawn_pos, 1.0F, GOLD, RED, scale, count);
    //             particles3d_add_effect_spiral           (spawn_pos, 2.0F, 5.0F, DARKBLUE, CYAN, scale, count);
    //             particles3d_add_effect_fountain         (spawn_pos, 0.5F, 15.0F, SKYBLUE, BLUE, scale, count);
    //             particles3d_add_effect_trail            (spawn_pos, {0.0F, 5.0F, 0.0F}, ORANGE, YELLOW, scale, count);
    //             particles3d_add_effect_dust_cloud       (spawn_pos, 2.0F, BROWN, BEIGE, scale, count);
    //             particles3d_add_effect_magic_burst      (spawn_pos, VIOLET, PINK, scale, count);
    //             particles3d_add_effect_debris           (spawn_pos, {0.0F, 10.0F, 0.0F}, GRAY, DARKGRAY, scale, count);
    //             particles3d_add_effect_chaos_stress_test(spawn_pos, 5.0F, count);
    //         }
    //     }
    // }

    // if (is_mouse_down(I_MOUSE_RIGHT) && !dbg_is_mouse_over_or_dragged()) {
    //     Rectangle const spawn_rect     = {mouse_pos.x - 20.0F, mouse_pos.y - 10.0F, 40.0F, 20.0F};
    //     F32 const particles_per_second = 15000.0F;
    //     SZ const count                 = (SZ)glm::round(particles_per_second * dt);
    //     F32 const scale                = 3.0F;

    //     if (count > 0) {
    //         particles2d_add_effect_explosion(spawn_rect, PURPLE, RED, scale, count);
    //         particles2d_add_effect_smoke    (spawn_rect, LIME, WHITE, scale, count);
    //         particles2d_add_effect_sparkle  (spawn_rect, GOLD, GREEN, scale, count);
    //         particles2d_add_effect_fire     (spawn_rect, GOLD, RED, scale, count);
    //         particles2d_add_effect_spiral   (spawn_rect, DARKBLUE, CYAN, scale, count);
    //     }
    // }
}

SCENE_DRAW(collision_test) {
    Vector2 const res = render_get_render_resolution();
    Camera3D *other_camera = g_world->player.non_player_camera ? &g_world->player.camera3d : c3d_get_default_ptr();

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

    RMODE_BEGIN(RMODE_3D) {
        world_draw_3d();
    } RMODE_END;

    RMODE_BEGIN(RMODE_3D_SKETCH) {
        if (c_render__skybox) { d3d_skybox(c_render__skybox_night ? asset_get_skybox("night.hdr") : asset_get_skybox("day.hdr")); }

        d3d_terrain_ex(asset_get_terrain("terrain2",{(F32)A_TERRAIN_DEFAULT_SIZE, (F32)A_TERRAIN_DEFAULT_SIZE, (F32)A_TERRAIN_DEFAULT_SIZE}),
                       {}, {A_TERRAIN_DEFAULT_SCALE, A_TERRAIN_DEFAULT_SCALE, A_TERRAIN_DEFAULT_SCALE}, WHITE);

        world_draw_3d_sketch();
        particles3d_draw();
    } RMODE_END;


    RMODE_BEGIN(RMODE_3D_HUD_SKETCH) {
        if (!g_world->player.non_player_camera) { player_draw_3d_hud(&g_world->player); }

        world_draw_3d_hud();
    } RMODE_END;

    RMODE_BEGIN(RMODE_3D_HUD) {
        if (!g_world->player.non_player_camera) { player_draw_3d_hud(&g_world->player); }
    } RMODE_END;

    RMODE_BEGIN(RMODE_3D_DBG) {
        if (c_debug__enabled) {
            if (g_world->player.non_player_camera) { player_draw_3d_dbg(&g_world->player); }
            world_draw_3d_dbg();
            lighting_draw_3d_dbg();
            render_draw_3d_dbg(other_camera);
            audio_draw_3d_dbg();

            // Draw collision meshes for entities with triangle collision
            // if (entity_is_valid(s.sponza_entity)) {
            //     AModel *sponza_model = asset_get_model(s.sponza_model_name);
            //     if (sponza_model && sponza_model->collision.generated) {
            //         Vector3 const pos = g_world->position[s.sponza_entity];
            //         Vector3 const scale = g_world->scale[s.sponza_entity];
            //         d3d_collision_mesh(&sponza_model->collision, pos, scale, GREEN);
            //     }
            // }
        }
    }
    RMODE_END;

    RMODE_BEGIN(RMODE_2D) {
        world_draw_2d();
    } RMODE_END;

    RMODE_BEGIN(RMODE_2D_SKETCH) {
        particles2d_draw();
    } RMODE_END;

    RMODE_BEGIN(RMODE_2D_HUD) {
        world_draw_2d_hud();

        if (s.fade_out.enabled) { d2d_rectangle_rec({0.0F, 0.0F, res.x, res.y}, Fade(BLACK, s.fade_out.duration)); }

        if (g_world->player.non_player_camera) {
            d2d_rectangle_v({}, res, Fade(MAGENTA, 0.25F));
            C8 const *text                = "NON-PLAYER CAMERA ACTIVE";
            AFont *font                   = asset_get_font("GoMono", ui_font_size(2.0F));
            Vector2 const text_size       = measure_text(font, text);
            Vector2 const center_position = {(res.x / 2.0F) - (text_size.x / 2.0F), (((F32)font->font_size + text_size.y) / 2.0F) + ui_scale_y(15.0F)};
            F32 const padding             = ui_scale_y(1.0F);
            Rectangle const bg_rec        = {
                center_position.x - padding,
                center_position.y - padding,
                text_size.x + (padding * 2.0F),
                text_size.y + (padding * 2.0F),
            };
            d2d_rectangle_rec(bg_rec, Fade(BLACK, 0.85F));
            d2d_text(font, text, center_position, SALMON);
        }

        screen_fade_draw_2d_hud();
        hud_draw_2d_hud();
        menu_draw_2d_hud(&s.menu);
    } RMODE_END;

    RMODE_BEGIN(RMODE_2D_HUD_SKETCH) {
        hud_draw_2d_hud_sketch();
    } RMODE_END;

    RMODE_BEGIN(RMODE_2D_DBG) {
        if (c_debug__enabled) {
            world_draw_2d_dbg();
            lighting_draw_2d_dbg();
            render_draw_2d_dbg(other_camera);
        }

        menu_draw_2d_dbg(&s.menu);
    } RMODE_END;
}
