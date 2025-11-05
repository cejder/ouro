#pragma once

// NOTE: Fog structure has been moved to world.hpp
// This header is kept for backward compatibility but all fog
// functions now operate on g_world->fog

#include "common.hpp"
#include <raylib.h>

fwd_decl(AShader);

void fog_init(AShader *shader);
void fog_update();
