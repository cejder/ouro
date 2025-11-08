#include "fog.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "common.hpp"
#include "render.hpp"

Fog g_fog = {};

void fog_init() {
    g_fog.density = 0.0F;
    g_fog.color   = BLACK;
}

void fog_update() {
    F32 color_f32[4];
    color_to_vec4(g_fog.color, color_f32);

    RenderModelShader* ms = &g_render.model_shader;
    SetShaderValue(ms->shader->base, ms->fog.density_loc, &g_fog.density, SHADER_UNIFORM_FLOAT);
    SetShaderValue(ms->shader->base, ms->fog.color_loc, color_f32, SHADER_UNIFORM_VEC4);

    RenderModelInstancedShader* mis = &g_render.model_instanced_shader;
    SetShaderValue(mis->shader->base, mis->fog.density_loc, &g_fog.density, SHADER_UNIFORM_FLOAT);
    SetShaderValue(mis->shader->base, mis->fog.color_loc, color_f32, SHADER_UNIFORM_VEC4);
}
