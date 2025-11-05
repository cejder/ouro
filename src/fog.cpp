#include "fog.hpp"
#include "world.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "common.hpp"

void fog_init(AShader *shader) {
    g_world->fog.shader      = shader;
    g_world->fog.density     = 0.0F;
    g_world->fog.density_loc = GetShaderLocation(shader->base, "fog.density");
    g_world->fog.color_loc   = GetShaderLocation(shader->base, "fog.color");
    g_world->fog.color       = BLACK;
}

void fog_update() {
    F32 color_f32[4];
    color_to_vec4(g_world->fog.color, color_f32);

    SetShaderValue(g_world->fog.shader->base, g_world->fog.density_loc, &g_world->fog.density, SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_world->fog.shader->base, g_world->fog.color_loc, color_f32, SHADER_UNIFORM_VEC4);
}
