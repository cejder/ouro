#include "render.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "cvar.hpp"
#include "log.hpp"
#include "message.hpp"
#include "particles_2d.hpp"
#include "particles_3d.hpp"
#include "profiler.hpp"
#include "render_tooltip.hpp"
#include "scene.hpp"
#include "string.hpp"
#include "time.hpp"
#include "world.hpp"

#include <raymath.h>
#include <rlgl.h>

Render g_render = {};

C8 static const *i_render_mode_names[RMODE_COUNT] = {
    "3D",
    "3D SKETCH",

    "2D",
    "2D SKETCH",

    "3D HUD",
    "3D HUD SKETCH",

    "2D HUD",
    "2D HUD SKETCH",

    "3D DBG",
    "2D DBG",

    "LAST LAYER",
};

// Create a render texture with writable depth buffer
RenderTexture static i_create_render_texture_with_depth(S32 width, S32 height) {
    RenderTexture target = {};
    target.id = rlLoadFramebuffer();  // Load an empty framebuffer
    if (target.id == 0) {
        lle("could not create new frame buffer");
        return target;
    }

    rlEnableFramebuffer(target.id);
    rlClearScreenBuffers();

    // Create color texture (default to RGBA)
    target.texture.id      = rlLoadTexture(nullptr, width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    target.texture.width   = width;
    target.texture.height  = height;
    target.texture.format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    target.texture.mipmaps = 1;

    rlTextureParameters(target.texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_ANISOTROPIC_16X);
    rlTextureParameters(target.texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);

    // Create depth texture buffer instead of default renderbuffer
    target.depth.id      = rlLoadTextureDepth(width, height, false);
    target.depth.width   = width;
    target.depth.height  = height;
    target.depth.format  = 19;  // DEPTH_COMPONENT_24BIT
    target.depth.mipmaps = 1;

    // Attach color texture and depth texture to FBO
    rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

    // Check if fbo is complete with attachments (valid)
    if (!rlFramebufferComplete(target.id)) { lle("failed to create frame buffer object"); }
    rlDisableFramebuffer();

    return target;
}

void render_init() {
    g_render.default_material = LoadMaterialDefault();

    RenderSkyboxShader *ss = &g_render.skybox_shader;
    ss->shader             = asset_get_shader("skybox");;
    ss->ambient_color_loc  = GetShaderLocation(ss->shader->base, "ambient");

    RenderSketchEffect *se = &g_render.sketch;
    se->shader             = asset_get_shader("sketch");;
    se->resolution_loc     = GetShaderLocation(se->shader->base, "resolution");
    se->major_color_loc    = GetShaderLocation(se->shader->base, "majorColor");
    se->minor_color_loc    = GetShaderLocation(se->shader->base, "minorColor");
    se->time_loc           = GetShaderLocation(se->shader->base, "time");

    RenderModelShader *ms     = &g_render.model_shader;
    ms->shader                = asset_get_shader(A_MODEL_SHADER_NAME);
    ms->animation_enabled_loc = GetShaderLocation(ms->shader->base, "animationEnabled");
    ms->view_pos_loc          = GetShaderLocation(ms->shader->base, "viewPos");
    ms->ambient_color_loc     = GetShaderLocation(ms->shader->base, "ambient");
    ms->fog.density_loc       = GetShaderLocation(ms->shader->base, "fog.density");
    ms->fog.color_loc         = GetShaderLocation(ms->shader->base, "fog.color");
    for (SZ i  = 0; i < LIGHTS_MAX; ++i) {
        ms->light[i].enabled_loc      = GetShaderLocation(ms->shader->base, TS("lights[%zu].enabled", i)->c);
        ms->light[i].type_loc         = GetShaderLocation(ms->shader->base, TS("lights[%zu].type", i)->c);
        ms->light[i].position_loc     = GetShaderLocation(ms->shader->base, TS("lights[%zu].position", i)->c);
        ms->light[i].direction_loc    = GetShaderLocation(ms->shader->base, TS("lights[%zu].direction", i)->c);
        ms->light[i].color_loc        = GetShaderLocation(ms->shader->base, TS("lights[%zu].color", i)->c);
        ms->light[i].intensity_loc    = GetShaderLocation(ms->shader->base, TS("lights[%zu].intensity", i)->c);
        ms->light[i].inner_cutoff_loc = GetShaderLocation(ms->shader->base, TS("lights[%zu].inner_cutoff", i)->c);
        ms->light[i].outer_cutoff_loc = GetShaderLocation(ms->shader->base, TS("lights[%zu].outer_cutoff", i)->c);
    }

    RenderModelInstancedShader *mis = &g_render.model_instanced_shader;
    mis->shader                     = asset_get_shader(A_MODEL_INSTANCED_SHADER_NAME);
    mis->mvp_loc                    = GetShaderLocation(mis->shader->base, "mvp");
    mis->view_pos_loc               = GetShaderLocation(mis->shader->base, "viewPos");
    mis->ambient_color_loc          = GetShaderLocation(mis->shader->base, "ambient");
    mis->instance_tint_loc          = GetShaderLocationAttrib(mis->shader->base, "instanceTint");
    mis->fog.density_loc            = GetShaderLocation(mis->shader->base, "fog.density");
    mis->fog.color_loc              = GetShaderLocation(mis->shader->base, "fog.color");
    for (SZ i = 0; i < LIGHTS_MAX; ++i) {
        mis->light[i].enabled_loc      = GetShaderLocation(mis->shader->base, TS("lights[%zu].enabled", i)->c);
        mis->light[i].type_loc         = GetShaderLocation(mis->shader->base, TS("lights[%zu].type", i)->c);
        mis->light[i].position_loc     = GetShaderLocation(mis->shader->base, TS("lights[%zu].position", i)->c);
        mis->light[i].direction_loc    = GetShaderLocation(mis->shader->base, TS("lights[%zu].direction", i)->c);
        mis->light[i].color_loc        = GetShaderLocation(mis->shader->base, TS("lights[%zu].color", i)->c);
        mis->light[i].intensity_loc    = GetShaderLocation(mis->shader->base, TS("lights[%zu].intensity", i)->c);
        mis->light[i].inner_cutoff_loc = GetShaderLocation(mis->shader->base, TS("lights[%zu].inner_cutoff", i)->c);
        mis->light[i].outer_cutoff_loc = GetShaderLocation(mis->shader->base, TS("lights[%zu].outer_cutoff", i)->c);
    }

    render_sketch_set_major_color(RENDER_DEFAULT_MAJOR_COLOR);
    render_sketch_set_minor_color(RENDER_DEFAULT_MINOR_COLOR);
    render_set_accent_color(BLACK);

    lighting_init();
    fog_init();
    render_set_ambient_color(RENDER_DEFAULT_AMBIENT_COLOR);

    particles2d_init();
    particles3d_init();

    c3d_reset();
    c2d_reset();

    for (SZ i = 0; i < RMODE_COUNT; ++i) {
        g_render.rmode_data[i].tint_color = RENDER_DEFAULT_TINT_COLOR;
        g_render.rmode_order[i] = (RenderMode)i;
    }

    Vector2 const res = {(F32)c_video__window_resolution_width, (F32)c_video__window_resolution_height};
    render_update_render_resolution(res);
    render_update_window_resolution(res);

    rlSetClipPlanes(RENDER_NEAR_PLANE, RENDER_FAR_PLANE);
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE, RL_FUNC_ADD, RL_MAX);

    g_render.speaker_icon_texture = asset_get_texture("speaker.png");
    g_render.camera_icon_texture  = asset_get_texture("camera.png");

    g_render.default_font      = asset_get_font("GoMono", ui_font_size(RENDER_DEFAULT_FONT_PERC));
    g_render.default_crosshair = asset_get_texture("cursor_crosshair.png");

    g_render.initialized = true;
}

void static i_do_fboy(F32 dt) {
    F32 const change_time        = 1.0F;
    F32 static time_since_change = change_time;
    time_since_change -= dt;
    if (time_since_change <= 0.0F) {
        Color const random_color = color_random();
        render_sketch_set_major_color(random_color);
        render_sketch_set_minor_color(color_contrast(random_color));
        time_since_change = change_time;
    }
}

void static i_do_tboy(F32 dt) {
    // Similar to fboy but we interpolate the colors with an easing function.
    F32 static const change_time   = 5.0F;
    S32 const variation            = 10;           // The variation in the color.
    F32 static accumulated_time    = change_time;  // We start at change_time to get new colors immediately.
    Color static start_major_color = {};
    Color static start_minor_color = {};
    auto static end_major_color    = RENDER_DEFAULT_MAJOR_COLOR;
    auto static end_minor_color    = RENDER_DEFAULT_MINOR_COLOR;
    SZ static change_count         = 0;
    SZ const max_changes           = 100;

    // We reached change_time and need new colors.
    if (accumulated_time > change_time) {
        start_major_color = g_render.sketch.major_color;
        start_minor_color = g_render.sketch.minor_color;
        end_major_color   = color_variation(start_major_color, variation);
        end_minor_color   = color_variation(start_minor_color, variation);
        accumulated_time  = 0.0F;
        change_count++;

        if (change_count == max_changes) {
            // Reset the colors to the default colors to avoid a color explosion/deviating too much from the default colors.
            end_major_color = RENDER_DEFAULT_MAJOR_COLOR;
            end_minor_color = RENDER_DEFAULT_MINOR_COLOR;
            change_count = 0;
            mio("TBoy has reached the maximum number of color changes, resetting colors.", WHITE);
        }
    }

    // We did not reach change_time yet, so we interpolate the colors.
    Color const major_color = color_ease(EASE_IN_OUT_QUAD, accumulated_time, start_major_color, end_major_color, change_time);
    Color const minor_color = color_ease(EASE_IN_OUT_QUAD, accumulated_time, start_minor_color, end_minor_color, change_time);
    render_sketch_set_major_color(major_color);
    render_sketch_set_minor_color(minor_color);

    accumulated_time += dt;
}

void render_update(F32 dt) {
    F32 const new_width  = (F32)GetScreenWidth();
    F32 const new_height = (F32)GetScreenHeight();

    if (new_width != (F32)c_video__window_resolution_width || new_height != (F32)c_video__window_resolution_height) { render_update_window_resolution({new_width, new_height}); }

    // TRIPS
    if (c_render__fboy) { i_do_fboy(dt); }
    if (c_render__tboy) { i_do_tboy(dt); }
}

void render_update_window_resolution(Vector2 new_res) {
    llt("Updating window resolution to %.0f x %.0f", new_res.x, new_res.y);

    c_video__window_resolution_width  = (S32)new_res.x;
    c_video__window_resolution_height = (S32)new_res.y;
    g_render.aspect_ratio             = new_res.x / new_res.y;
}

void render_update_render_resolution(Vector2 new_res) {
    llt("Updating render resolution to %.0f x %.0f", new_res.x, new_res.y);

    UnloadRenderTexture(g_render.final_render_target);
    g_render.final_render_target = i_create_render_texture_with_depth((S32)new_res.x, (S32)new_res.y);

    c_video__render_resolution_width        = (S32)new_res.x;
    c_video__render_resolution_height       = (S32)new_res.y;
    g_render.cameras.c2d.default_cam.offset = {new_res.x / 2.0F, new_res.y / 2.0F};
    g_render.cameras.c2d.default_cam.target = {new_res.x / 2.0F, new_res.y / 2.0F};

    for (SZ i = 0; i < RMODE_COUNT; ++i) {
        UnloadRenderTexture(g_render.rmode_data[i].target);

        BOOL const is_3d_mode = (i == RMODE_3D || i == RMODE_3D_SKETCH || i == RMODE_3D_HUD || i == RMODE_3D_HUD_SKETCH || i == RMODE_3D_DBG);
        if (is_3d_mode) {
            g_render.rmode_data[i].target = i_create_render_texture_with_depth((S32)new_res.x, (S32)new_res.y);
        } else {
            g_render.rmode_data[i].target = LoadRenderTexture((S32)new_res.x, (S32)new_res.y);
            SetTextureFilter(g_render.rmode_data[i].target.texture, TEXTURE_FILTER_POINT);
            SetTextureFilter(g_render.rmode_data[i].target.depth, TEXTURE_FILTER_POINT);
        }
    }

    F32 time = time_get();
    F32 major_color[4];
    F32 minor_color[4];

    color_to_vec4(g_render.sketch.major_color, major_color);
    color_to_vec4(g_render.sketch.minor_color, minor_color);

    SetShaderValue(g_render.sketch.shader->base, g_render.sketch.time_loc, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_render.sketch.shader->base, g_render.sketch.resolution_loc, (F32[2]){new_res.x, new_res.y}, SHADER_UNIFORM_VEC2);
    SetShaderValue(g_render.sketch.shader->base, g_render.sketch.major_color_loc, major_color, SHADER_UNIFORM_VEC4);
    SetShaderValue(g_render.sketch.shader->base, g_render.sketch.minor_color_loc, minor_color, SHADER_UNIFORM_VEC4);

    // Also recreate the default font size
    g_render.default_font = asset_get_font("GoMono", ui_font_size(RENDER_DEFAULT_FONT_PERC));
}

// UI scaling helpers
S32 ui_font_size(F32 percentage) {
    return (S32)((F32)c_video__render_resolution_height * (percentage / 100.0F));
}

F32 ui_scale_x(F32 percentage) {
    return (F32)c_video__render_resolution_width * (percentage / 100.0F);
}

F32 ui_scale_y(F32 percentage) {
    return (F32)c_video__render_resolution_height * (percentage / 100.0F);
}

Vector2 ui_shadow_offset(F32 x_perc, F32 y_perc) {
    return {ui_scale_x(x_perc), ui_scale_y(y_perc)};
}

void render_begin() {
    lighting_update(g_render.cameras.c3d.active_cam);
    fog_update();

    ClearBackground(BLANK);
    BeginDrawing();
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);

    for (SZ i = 0; i < RMODE_COUNT; ++i) { g_render.rmode_order[i] = (RenderMode)i; }
}

void render_begin_render_mode(RenderMode mode) {
    PBEGIN(render_mode_to_cstr(mode));
    PBEGIN("BODY_BEGIN_RENDER_MODE");

    g_render.begun_rmode = mode;
    g_render.rmode_data[mode].begun_but_not_ended = true;

    BeginTextureMode(g_render.rmode_data[mode].target);
    ClearBackground(BLANK);

    // Enable wireframe mode if necessary
    if (g_render.wireframe_mode && (mode == RMODE_3D || mode == RMODE_3D_SKETCH || mode == RMODE_3D_HUD || mode == RMODE_3D_HUD_SKETCH)) {
        rlEnableWireMode();
        rlSetLineWidth(ui_scale_x(RENDER_WIREFRAME_LINE_THICKNESS_PERC));
    } else if (mode == RMODE_LAST_LAYER) {
        rlSetLineWidth(1.0F);
    } else {
        rlSetLineWidth(ui_scale_x(RENDER_DEFAULT_LINE_THICKNESS_PERC));
    }

    switch (mode) {
        case RMODE_3D:
        case RMODE_3D_SKETCH:
        case RMODE_3D_HUD:
        case RMODE_3D_HUD_SKETCH:
        case RMODE_3D_DBG: {
            BeginMode3D(*g_render.cameras.c3d.active_cam);
        } break;

        case RMODE_2D:
        case RMODE_2D_SKETCH: {
            BeginMode2D(*g_render.cameras.c2d.active_cam);
        } break;

        case RMODE_2D_HUD:
        case RMODE_2D_HUD_SKETCH:
        case RMODE_2D_DBG:
        case RMODE_LAST_LAYER: {
            // Nothing to do
        } break;

        default: {
            lle("Unknown render mode %d", mode);
        } break;
    }

    PEND("BODY_BEGIN_RENDER_MODE");
}

void render_end_render_mode() {
    PBEGIN("BODY_END_RENDER_MODE");
    RenderMode const mode = g_render.begun_rmode;

    g_render.rmode_data[mode].begun_but_not_ended = false;

    switch (mode) {
        case RMODE_3D:
        case RMODE_3D_SKETCH:
        case RMODE_3D_HUD:
        case RMODE_3D_HUD_SKETCH:
        case RMODE_3D_DBG: {
            EndMode3D();
            EndTextureMode();
        } break;

        case RMODE_2D:
        case RMODE_2D_SKETCH: {
            EndMode2D();
            EndTextureMode();
        } break;

        case RMODE_2D_HUD:
        case RMODE_2D_HUD_SKETCH:
        case RMODE_2D_DBG:
        case RMODE_LAST_LAYER: {
            EndTextureMode();
        } break;

        default: {
            lle("Unknown render mode %d", mode);
        } break;
    }

    // Disable wireframe mode if necessary.
    if (g_render.wireframe_mode && (mode == RMODE_3D || mode == RMODE_3D_SKETCH || mode == RMODE_3D_HUD || mode == RMODE_3D_HUD_SKETCH)) {
        rlDisableWireMode();
    }

    // Check if we had any draw calls in this mode, if yes, update the generation.
    if (g_render.rmode_data[mode].draw_call_count > 0) { g_render.rmode_data[mode].generation = g_profiler.current_generation; }

    PEND("BODY_END_RENDER_MODE");
    PEND(render_mode_to_cstr(mode));
}

void static i_set_uniforms() {
    F32 const current_time = time_get();

    // SKETCH
    SetShaderValue(g_render.sketch.shader->base, g_render.sketch.time_loc, &current_time, SHADER_UNIFORM_FLOAT);
}

void render_end() {
    RenderTexture *final_target = &g_render.final_render_target;
    BeginTextureMode(*final_target);
    ClearBackground(scenes_get_clear_color());

    Vector2 const render_res = render_get_render_resolution();
    Vector2 const window_res = render_get_window_resolution();
    Rectangle src            = {0.0F, 0.0F, render_res.x, -render_res.y};
    Rectangle dst            = {0.0F, 0.0F, render_res.x,  render_res.y};

    i_set_uniforms();

    for (auto mode : g_render.rmode_order) {
        if (g_render.rmode_data[mode].begun_but_not_ended) {
            lle("Render mode %s was not ended", render_mode_to_cstr(mode));
            return;
        }

        if (g_render.rmode_data[mode].draw_call_count > 0) {
            BOOL const should_use_sketch = c_render__sketch_effect && (
                mode == RMODE_3D_SKETCH     ||
                mode == RMODE_3D_HUD_SKETCH ||
                mode == RMODE_2D_SKETCH     ||
                mode == RMODE_2D_HUD_SKETCH );

            if (should_use_sketch) { BeginShaderMode(g_render.sketch.shader->base); }

            DrawTexturePro(g_render.rmode_data[mode].target.texture, src, dst, {}, 0.0F, g_render.rmode_data[mode].tint_color);

            if (should_use_sketch) { EndShaderMode(); }
        }
    }

    EndTextureMode();

    src = {0.0F, 0.0F, render_res.x, -render_res.y};
    dst = {0.0F, 0.0F, window_res.x,  window_res.y};
    DrawTexturePro(final_target->texture, src, dst, {}, 0.0F, WHITE);
}

void render_post() {
    PBEGIN("BODY_RENDER_POST");

    EndBlendMode();
    EndDrawing();

    for (auto mode : g_render.rmode_order) {
        g_render.rmode_data[mode].previous_draw_call_count = g_render.rmode_data[mode].draw_call_count;
        g_render.rmode_data[mode].draw_call_count = 0;
    }

    PEND("BODY_RENDER_POST");
}

void render_draw_2d_dbg(Camera3D *camera) {
    if (c_debug__camera_info) {
        F32 const distance_to_camera = Vector3Distance(camera->position, g_render.cameras.c3d.active_cam->position);
        if (distance_to_camera > 15.0F) { return; }
        if (!c3d_is_point_in_frustum(camera->position)) { return; }

        AFont *font = g_render.default_font;

        RenderTooltip tt = {};
        render_tooltip_init(font, &tt, TS("  "), camera->position, true);

        rib_V3(&tt,  "\\ouc{#9db4c0ff}POSITION", camera->position);
        rib_V3(&tt,  "\\ouc{#9db4c0ff}TARGET",   camera->target);
        rib_V3(&tt,  "\\ouc{#9db4c0ff}UP",       camera->up);
        rib_F32(&tt, "\\ouc{#9db4c0ff}FOVY",     camera->fovy);
        rib_F32(&tt, "\\ouc{#9db4c0ff}DISTANCE", distance_to_camera);

        render_tooltip_draw(&tt);
    }
}

void render_draw_3d_dbg(Camera3D *camera) {
    if (c_debug__camera_info) {
        d3d_camera(camera, RED);
    }

    if (c_debug__terrain_info) {
        // Terrain height debug visualization
        ATerrain* terrain = g_world->base_terrain;

        U32 r = A_TERRAIN_DEFAULT_SIZE / A_TERRAIN_SAMPLE_RATE;  // NOLINT

        // Build batch transforms
        SZ idx                   = 0;
        U32 const sample_stride  = 4;
        Color const normal_color = BEIGE;
        MatrixArray batch_transforms;
        array_init(MEMORY_TYPE_ARENA_TRANSIENT, &batch_transforms, (SZ)A_TERRAIN_DEFAULT_SIZE * (SZ)A_TERRAIN_DEFAULT_SIZE);

        for (U32 z = 0U; z < A_TERRAIN_DEFAULT_SIZE; z += r) {
            if ((z / r) % sample_stride != 0) {
                idx += A_TERRAIN_DEFAULT_SIZE / r;
                continue;
            }
            for (U32 x = 0U; x < A_TERRAIN_DEFAULT_SIZE; x += r) {
                if ((x / r) % sample_stride == 0) {
                    array_push(&batch_transforms, array_get(&terrain->info_transforms, idx));

                    // Draw normal vector
                    Vector3 const start = array_get(&terrain->info_positions, idx);
                    Vector3 const end   = Vector3Add(start, array_get(&terrain->info_normals, idx));
                    d3d_line(start, end, RENDER_DEFAULT_NORMAL_COLOR);
                }
                idx++;
            }
        }

        d3d_mesh_rl_instanced(&terrain->info_mesh, &terrain->info_material, batch_transforms.data, batch_transforms.count);
    }
}

Vector2 render_get_center_of_render() {
    return {(F32)c_video__render_resolution_width / 2.0F, (F32)c_video__render_resolution_width / 2.0F};
}

Vector2 render_get_center_of_window() {
    return {(F32)c_video__window_resolution_width / 2.0F, (F32)c_video__window_resolution_width / 2.0F};
}

Vector2 render_get_render_resolution() {
    return {(F32)c_video__render_resolution_width, (F32)c_video__render_resolution_height};
}

Vector2 render_get_window_resolution() {
    return {(F32)c_video__window_resolution_width, (F32)c_video__window_resolution_height};
}

void render_set_accent_color(Color color) {
    g_render.accent_color = color;
}

void render_set_ambient_color(Color color) {
    color_to_vec4(color, g_render.ambient_color);
    SetShaderValue(g_render.model_shader.shader->base,           g_render.model_shader.ambient_color_loc,           g_render.ambient_color, SHADER_UNIFORM_VEC4);
    SetShaderValue(g_render.model_instanced_shader.shader->base, g_render.model_instanced_shader.ambient_color_loc, g_render.ambient_color, SHADER_UNIFORM_VEC4);
    SetShaderValue(g_render.skybox_shader.shader->base,          g_render.skybox_shader.ambient_color_loc,          g_render.ambient_color, SHADER_UNIFORM_VEC4);
}

Color render_get_ambient_color() {
    Color color = {};
    color_from_vec4(&color, g_render.ambient_color);
    return color;
}

void render_sketch_set_major_color(Color color) {
    g_render.sketch.major_color = color;

    F32 major_color[4];
    color_to_vec4(color, major_color);
    SetShaderValue(g_render.sketch.shader->base, g_render.sketch.major_color_loc, major_color, SHADER_UNIFORM_VEC4);
}

void render_sketch_set_minor_color(Color color) {
    g_render.sketch.minor_color = color;

    F32 minor_color[4];
    color_to_vec4(color, minor_color);
    SetShaderValue(g_render.sketch.shader->base, g_render.sketch.minor_color_loc, minor_color, SHADER_UNIFORM_VEC4);
}

void render_vary_sketch_colors(S32 variation) {
    Color static start_major_color = {};
    Color static start_minor_color = {};
    auto static end_major_color    = RENDER_DEFAULT_MAJOR_COLOR;
    auto static end_minor_color    = RENDER_DEFAULT_MINOR_COLOR;
    SZ static change_count         = 0;
    SZ const max_changes           = 1000;
    F32 const min_variance         = 0.25F;

    start_major_color = g_render.sketch.major_color;
    start_minor_color = g_render.sketch.minor_color;
    end_major_color   = color_variation(start_major_color, variation);
    end_minor_color   = color_variation(start_minor_color, variation);
    change_count++;

    BOOL const too_low_variance = color_get_variance(end_major_color, end_minor_color) < min_variance;
    BOOL const max_change_reached = change_count == max_changes;

    if (too_low_variance || max_change_reached) {
        end_major_color = RENDER_DEFAULT_MAJOR_COLOR;
        end_minor_color = RENDER_DEFAULT_MINOR_COLOR;
        change_count    = 0;
        mio("Sketch color shift has reached the conditional limit, resetting ...", WHITE);
    }

    render_sketch_set_major_color(end_major_color);
    render_sketch_set_minor_color(end_minor_color);
}

void render_sketch_swap_colors() {
    Color const t = g_render.sketch.major_color;
    render_sketch_set_major_color(g_render.sketch.minor_color);
    render_sketch_set_minor_color(t);
}

C8 const *render_mode_to_cstr(RenderMode mode) {
    return i_render_mode_names[mode];
}

void render_set_render_mode_order_for_frame(RenderModeOrderTarget target) {
#ifdef OURO_DEVEL
    // Check that all modes are present
    BOOL seen[RMODE_COUNT] = {false};

    for (auto mode : target.order) {
        // Check if the mode is valid
        _assert_(mode < RMODE_COUNT, "Invalid render mode in order");
        seen[mode] = true;
    }

    // Check if all modes are present (this is the important check)
    for (SZ i = 0; i < RMODE_COUNT - 1; ++i) {
        _assertf_(seen[i], "Missing render mode in order: '%s' [%zu] in scene '%s'", render_mode_to_cstr((RenderMode)i), i, scenes_current_scene_to_cstr());
    }
#endif

    for (SZ i = 0; i < RMODE_COUNT; ++i) { g_render.rmode_order[i] = target.order[i]; }
}

RenderModeOrderTarget render_get_default_render_mode_order() {
    // NOTE: This needs to be extended when we add new targets. Kinda ugly.
    RenderModeOrderTarget target = {
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
    };

#ifdef OURO_DEVEL
    // Check that all modes are present exactly once
    BOOL seen[RMODE_COUNT] = {false};

    for (auto mode : target.order) {
        _assert_(mode < RMODE_COUNT, "Invalid render mode in order");
        _assert_(!seen[mode], "Duplicate render mode in order");
        seen[mode] = true;
    }

    // Verify all modes are present
    for (SZ i = 0; i < RMODE_COUNT - 1; ++i) { _assert_(seen[i], "Missing render mode in order"); }
#endif

    return target;
}
