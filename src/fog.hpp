#pragma once

#include "common.hpp"

#include <raylib.h>

struct FogUniforms {
    S32 density_loc;
    S32 color_loc;
};

struct Fog {
    F32 density;
    Color color;
};

Fog extern g_fog;

void fog_init();
void fog_update();
