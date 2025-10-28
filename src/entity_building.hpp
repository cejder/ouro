#pragma once

#include "common.hpp"

#include <raylib.h>

fwd_decl(World);

struct EntityBuildingLumberyard {
    SZ wood_count;
    Vector3 target_scale;
    Vector3 start_scale;
    F32 scale_anim_elapsed;
    F32 scale_anim_duration;
    F32 particle_spawn_accumulator;
    BOOL is_animating_scale;
};

struct EntityBuilding {
    union {
        EntityBuildingLumberyard lumberyard;
    };
};

void entity_building_init();
void entity_building_update(EID id, F32 dt);
