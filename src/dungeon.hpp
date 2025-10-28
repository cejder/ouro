#pragma once

#include "common.hpp"

fwd_decl(Player);

void dungeon_draw_3d_sketch();
void dungeon_draw_2d_dbg();
void dungeon_update(Player *player, F32 dt);
