#pragma once

#include "common.hpp"

#include <raylib.h>

fwd_decl(AShader);

struct Fog {
    AShader *shader;
    F32 density;
    Color color;
    S32 density_loc;
    S32 color_loc;

    AShader *shader_instanced;
    S32 density_loc_instanced;
    S32 color_loc_instanced;
};

Fog extern g_fog;

void fog_init(AShader *shader, AShader *shader_instanced);
void fog_update();
