#pragma once

#include "common.hpp"

#include <raylib.h>

void dungeon_update(F32 dt);
void dungeon_draw_3d_sketch();
void dungeon_draw_2d_dbg();
BOOL dungeon_is_entity_occluded(EID entity_id);
