#include "fog.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "common.hpp"

Fog g_fog = {};

void fog_init(AShader *shader, AShader *shader_instanced) {
    g_fog.shader      = shader;
    g_fog.density     = 0.0F;
    g_fog.density_loc = GetShaderLocation(shader->base, "fog.density");
    g_fog.color_loc   = GetShaderLocation(shader->base, "fog.color");
    g_fog.color       = BLACK;

    g_fog.shader_instanced          = shader_instanced;
    g_fog.density_loc_instanced     = GetShaderLocation(shader_instanced->base, "fog.density");
    g_fog.color_loc_instanced       = GetShaderLocation(shader_instanced->base, "fog.color");
}

void fog_update() {
    F32 color_f32[4];
    color_to_vec4(g_fog.color, color_f32);

    SetShaderValue(g_fog.shader->base, g_fog.density_loc, &g_fog.density, SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_fog.shader->base, g_fog.color_loc, color_f32, SHADER_UNIFORM_VEC4);

    SetShaderValue(g_fog.shader_instanced->base, g_fog.density_loc_instanced, &g_fog.density, SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_fog.shader_instanced->base, g_fog.color_loc_instanced, color_f32, SHADER_UNIFORM_VEC4);
}
