#include "light.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "cvar.hpp"
#include "log.hpp"
#include "profiler.hpp"
#include "render.hpp"
#include "render_tooltip.hpp"
#include "string.hpp"

#include <raymath.h>

Lighting g_lighting = {};

void lighting_init() {
    lighting_default_lights_setup();

    g_lighting.initialized = true;
}

void lighting_disable_all_lights() {
    for (auto &light : g_lighting.lights) {
        light.enabled = false;
    }
}

void lighting_set_point_light(SZ idx, BOOL enabled, Vector3 position, Color color, F32 intensity) {
    Light *light = &g_lighting.lights[idx];

    light->enabled      = enabled;
    light->type         = LIGHT_TYPE_POINT;
    light->position     = position;
    light->direction    = {0.0F, 0.0F, 0.0F};
    light->intensity    = intensity;
    light->inner_cutoff = 0.0F;
    light->outer_cutoff = 0.0F;
    light->dirty        = true;
    color_to_vec4(color, light->color);
}

void lighting_set_spot_light(SZ idx, BOOL enabled, Vector3 position, Vector3 direction, Color color, F32 intensity, F32 inner_cutoff, F32 outer_cutoff) {
    Light *light = &g_lighting.lights[idx];

    light->enabled      = enabled;
    light->type         = LIGHT_TYPE_SPOT;
    light->position     = position;
    light->direction    = Vector3Normalize(direction);
    light->intensity    = intensity;
    light->inner_cutoff = inner_cutoff;
    light->outer_cutoff = outer_cutoff;
    light->dirty        = true;
    color_to_vec4(color, light->color);
}

void lighting_set_light_enabled(SZ idx, BOOL enabled) {
    g_lighting.lights[idx].enabled = enabled ? 1 : 0;
    g_lighting.lights[idx].dirty   = true;
}

void lighting_set_light_position(SZ idx, Vector3 position) {
    g_lighting.lights[idx].position = position;
    g_lighting.lights[idx].dirty    = true;
}

Color static i_overworld_light_colors[LIGHTS_MAX] = {
    RED, GREEN, BLUE, MAGENTA, CYAN,
    NAYBEIGE, BEIGE, TUSCAN, BEIGE, NAYBEIGE,
};

void lighting_default_lights_setup() {
    F32 const height     = 250.0F;
    Vector3 const center = {500.0F, height, 500.0F};
    F32 const half_size  = 500.0F;

    // Lights 0-4 are spotlights
    lighting_set_spot_light(0, false, {}, {}, i_overworld_light_colors[0], LIGHT_DEFAULT_INTENSITY, 0, 0);
    lighting_set_spot_light(1, false, {}, {}, i_overworld_light_colors[1], LIGHT_DEFAULT_INTENSITY, 0, 0);
    lighting_set_spot_light(2, false, {}, {}, i_overworld_light_colors[2], LIGHT_DEFAULT_INTENSITY, 0, 0);
    lighting_set_spot_light(3, false, {}, {}, i_overworld_light_colors[3], LIGHT_DEFAULT_INTENSITY, 0, 0);
    lighting_set_spot_light(4, false, {}, {}, i_overworld_light_colors[4], LIGHT_DEFAULT_INTENSITY, 0, 0);
    // Center light
    lighting_set_point_light(5, false, center, i_overworld_light_colors[5], LIGHT_DEFAULT_INTENSITY);
    // Corner lights using Vector3Add
    lighting_set_point_light(6, true, Vector3Add(center, (Vector3){half_size, height, half_size}), i_overworld_light_colors[6], LIGHT_DEFAULT_INTENSITY);    // (1000, 1000)
    lighting_set_point_light(7, true, Vector3Add(center, (Vector3){-half_size, height, half_size}), i_overworld_light_colors[7], LIGHT_DEFAULT_INTENSITY);   // (0, 1000)
    lighting_set_point_light(8, true, Vector3Add(center, (Vector3){-half_size, height, -half_size}), i_overworld_light_colors[8], LIGHT_DEFAULT_INTENSITY);  // (0, 0)
    lighting_set_point_light(9, true, Vector3Add(center, (Vector3){half_size, height, -half_size}), i_overworld_light_colors[9], LIGHT_DEFAULT_INTENSITY);   // (1000, 0)
}

void lighting_update(Camera3D *camera) {
    RenderModelShader *ms           = &g_render.model_shader;
    RenderModelInstancedShader *mis = &g_render.model_instanced_shader;
    RenderModelAnimatedInstancedShader *mais = &g_render.model_animated_instanced_shader;

    for (SZ i = 0; i < LIGHTS_MAX; ++i) {
        Light *light = &g_lighting.lights[i];
        light->in_frustum = c3d_is_point_in_frustum(light->position);

        if (!light->dirty) { continue; }
        light->dirty = false;

        SetShaderValue(ms->shader->base, ms->light[i].enabled_loc, &light->enabled, SHADER_UNIFORM_INT);
        SetShaderValue(ms->shader->base, ms->light[i].type_loc, &light->type, SHADER_UNIFORM_INT);
        SetShaderValue(ms->shader->base, ms->light[i].position_loc, &light->position, SHADER_UNIFORM_VEC3);
        SetShaderValue(ms->shader->base, ms->light[i].direction_loc, &light->direction, SHADER_UNIFORM_VEC3);
        SetShaderValue(ms->shader->base, ms->light[i].color_loc, light->color, SHADER_UNIFORM_VEC4);
        SetShaderValue(ms->shader->base, ms->light[i].intensity_loc, &light->intensity, SHADER_UNIFORM_FLOAT);
        SetShaderValue(ms->shader->base, ms->light[i].inner_cutoff_loc, &light->inner_cutoff, SHADER_UNIFORM_FLOAT);
        SetShaderValue(ms->shader->base, ms->light[i].outer_cutoff_loc, &light->outer_cutoff, SHADER_UNIFORM_FLOAT);

        SetShaderValue(mis->shader->base, mis->light[i].enabled_loc, &light->enabled, SHADER_UNIFORM_INT);
        SetShaderValue(mis->shader->base, mis->light[i].type_loc, &light->type, SHADER_UNIFORM_INT);
        SetShaderValue(mis->shader->base, mis->light[i].position_loc, &light->position, SHADER_UNIFORM_VEC3);
        SetShaderValue(mis->shader->base, mis->light[i].direction_loc, &light->direction, SHADER_UNIFORM_VEC3);
        SetShaderValue(mis->shader->base, mis->light[i].color_loc, light->color, SHADER_UNIFORM_VEC4);
        SetShaderValue(mis->shader->base, mis->light[i].intensity_loc, &light->intensity, SHADER_UNIFORM_FLOAT);
        SetShaderValue(mis->shader->base, mis->light[i].inner_cutoff_loc, &light->inner_cutoff, SHADER_UNIFORM_FLOAT);
        SetShaderValue(mis->shader->base, mis->light[i].outer_cutoff_loc, &light->outer_cutoff, SHADER_UNIFORM_FLOAT);

        SetShaderValue(mais->shader->base, mais->light[i].enabled_loc, &light->enabled, SHADER_UNIFORM_INT);
        SetShaderValue(mais->shader->base, mais->light[i].type_loc, &light->type, SHADER_UNIFORM_INT);
        SetShaderValue(mais->shader->base, mais->light[i].position_loc, &light->position, SHADER_UNIFORM_VEC3);
        SetShaderValue(mais->shader->base, mais->light[i].direction_loc, &light->direction, SHADER_UNIFORM_VEC3);
        SetShaderValue(mais->shader->base, mais->light[i].color_loc, light->color, SHADER_UNIFORM_VEC4);
        SetShaderValue(mais->shader->base, mais->light[i].intensity_loc, &light->intensity, SHADER_UNIFORM_FLOAT);
        SetShaderValue(mais->shader->base, mais->light[i].inner_cutoff_loc, &light->inner_cutoff, SHADER_UNIFORM_FLOAT);
        SetShaderValue(mais->shader->base, mais->light[i].outer_cutoff_loc, &light->outer_cutoff, SHADER_UNIFORM_FLOAT);
    }

    F32 camera_pos[3] = {camera->position.x, camera->position.y, camera->position.z};
    SetShaderValue(ms->shader->base, ms->view_pos_loc, camera_pos, SHADER_UNIFORM_VEC3);
    SetShaderValue(mis->shader->base, mis->view_pos_loc, camera_pos, SHADER_UNIFORM_VEC3);
    SetShaderValue(mais->shader->base, mais->view_pos_loc, camera_pos, SHADER_UNIFORM_VEC3);
}

ATexture static *i_get_icon(LightType type) {
    switch (type) {
        case LIGHT_TYPE_POINT:
            return asset_get_texture("light_point.png");
        case LIGHT_TYPE_SPOT:
            return asset_get_texture("light_spot.png");
        default:
            _unreachable_();
    }
}

void lighting_draw_2d_dbg() {
    if (!c_debug__light_info) { return; }

    AFont *font          = g_render.default_font;
    Camera const camera  = c3d_get();
    Vector2 const center = render_get_center_of_render();
    F32 const size       = 24.0F;
    F32 const padding    = 8.0F;
    F32 const spacing    = 4.0F;
    F32 const x_start    = center.x - ((size * (F32)LIGHTS_MAX + spacing * (F32)(LIGHTS_MAX - 1)) / 2.0F);
    F32 const y_start    = padding;

    for (SZ i = 0; i < LIGHTS_MAX; ++i) {
        F32 const x             = x_start + ((size + spacing) * (F32)(i % 5));
        F32 const y             = y_start + ((size + spacing) * (F32)(S32)(i / 5));
        Light *light            = &g_lighting.lights[i];
        Color const state_color = light->enabled ? GREEN : RED;
        Color light_color       = {};
        color_from_vec4(&light_color, light->color);

        d2d_rectangle((S32)x, (S32)y, (S32)size, (S32)size * 1 / 3, light_color);
        d2d_rectangle((S32)x, (S32)y + ((S32)size * 1 / 3), (S32)size, (S32)size * 2 / 3, state_color);

        ATexture *icon      = i_get_icon(light->type);
        Rectangle const src = {0.0F, 0.0F, (F32)icon->base.width, (F32)icon->base.height};
        Rectangle const dst = {x + ((size * 1 / 3) / 2.0F), y + (size * 1 / 3), size * 2 / 3, size * 2 / 3};
        d2d_texture_pro(icon, src, dst, {}, 0.0F, Fade(BLACK, 0.50F));

        F32 const distance_to_camera = Vector3Distance(light->position, camera.position);
        if (distance_to_camera > 15.0F) { continue; }
        if (!light->in_frustum)         { continue; }

        C8 light_color_hex[COLOR_HEX_SIZE] = {};
        color_hex_from_color(light_color, light_color_hex);

        RenderTooltip tt = {};
        render_tooltip_init(font, &tt, TS("  "), light->position, true);

        rib_V3(&tt,    "\\ouc{#9db4c0ff}POSITION",  light->position);
        rib_F32(&tt,   "\\ouc{#9db4c0ff}DISTANCE",  distance_to_camera);
        rib_COLOR(&tt, "\\ouc{#9db4c0ff}COLOR",     light_color_hex, light_color);
        rib_F32(&tt,   "\\ouc{#9db4c0ff}INTENSITY", light->intensity);

        render_tooltip_draw(&tt);
    }
}

void lighting_draw_3d_dbg() {
    if (!c_debug__light_info) { return; }

    Camera const camera = c3d_get();

    for (auto &light : g_lighting.lights) {
        if (!light.in_frustum) { continue; }

        Color color = {};
        color_from_vec4(&color, light.color);

        F32 const distance = Vector3Distance(camera.position, light.position);

        // More aggressive scaling with higher minimum size
        F32 const size = glm::clamp(distance * 0.05F, 0.4F, 2.5F);

        // Draw solid sphere
        d3d_sphere(light.position, size, ColorAlpha(color, 0.6F));

        // Draw wireframe for visibility
        d3d_sphere_wires(light.position, size * 1.1F, 8, 12, color);

        // Icon positioned towards camera
        Vector3 const to_camera     = Vector3Normalize(Vector3Subtract(camera.position, light.position));
        Vector3 const icon_position = Vector3Add(light.position, Vector3Scale(to_camera, size * 1.5F));

        d3d_billboard(i_get_icon(light.type), icon_position, size * 1.8F, light.enabled ? GREEN : RED);
    }
}

void lighting_dump() {
    for (SZ i = 0; i < LIGHTS_MAX; ++i) {
        Light *light = &g_lighting.lights[i];

        if (light->type == LIGHT_TYPE_POINT) {
            lln("\\ouc{#00ddddff}Light[%zu] POINT: %s | pos(%7.1f,%7.1f,%7.1f) | color(%.2f,%.2f,%.2f,%.2f) | intensity=%7.1f", i,
                light->enabled ? "ON " : "OFF", light->position.x, light->position.y, light->position.z, light->color[0], light->color[1], light->color[2],
                light->color[3], light->intensity);
        } else if (light->type == LIGHT_TYPE_SPOT) {
            lln("\\ouc{#ffaa00ff}Light[%zu] SPOT:  %s | pos(%7.1f,%7.1f,%7.1f) | color(%.2f,%.2f,%.2f,%.2f) | intensity=%7.1f | dir(%7.1f,%7.1f,%7.1f) | "
                "cutoff(%.1f,%.1f)",
                i, light->enabled ? "ON " : "OFF", light->position.x, light->position.y, light->position.z, light->color[0], light->color[1], light->color[2],
                light->color[3], light->intensity, light->direction.x, light->direction.y, light->direction.z, light->inner_cutoff, light->outer_cutoff);
        }
    }
}
