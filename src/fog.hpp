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
};

Fog extern g_fog;

void fog_init(AShader *shader);
void fog_update();
