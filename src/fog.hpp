#pragma once

#include "common.hpp"
#include <raylib.h>

fwd_decl(AShader);

void fog_init(AShader *shader);
void fog_update();
