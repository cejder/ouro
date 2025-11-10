#include "entity_building.hpp"

#include "assert.hpp"
#include "ease.hpp"
#include "entity.hpp"
#include "particles_3d.hpp"
#include "world.hpp"

struct IEntityContext {
} static i_ctx = {};

void entity_building_init() {}

void entity_building_update(EID id, F32 dt) {
    switch (g_world->type[id]) {
    case ENTITY_TYPE_BUILDING_LUMBERYARD: {
        EntityBuildingLumberyard *lumberyard = &g_world->building[id].lumberyard;

        // Update scale animation if active
        if (lumberyard->is_animating_scale) {
            lumberyard->scale_anim_elapsed += dt;

            if (lumberyard->scale_anim_elapsed >= lumberyard->scale_anim_duration) {
                // Animation complete - set final scale
                entity_set_scale(id, lumberyard->target_scale);
                lumberyard->is_animating_scale = false;
            } else {
                // Interpolate scale using ease-out-back for a bouncy effect
                Vector3 current_scale;
                current_scale.x = ease_out_back(lumberyard->scale_anim_elapsed,
                                                lumberyard->start_scale.x,
                                                lumberyard->target_scale.x - lumberyard->start_scale.x,
                                                lumberyard->scale_anim_duration);
                current_scale.y = ease_out_back(lumberyard->scale_anim_elapsed,
                                                lumberyard->start_scale.y,
                                                lumberyard->target_scale.y - lumberyard->start_scale.y,
                                                lumberyard->scale_anim_duration);
                current_scale.z = ease_out_back(lumberyard->scale_anim_elapsed,
                                                lumberyard->start_scale.z,
                                                lumberyard->target_scale.z - lumberyard->start_scale.z,
                                                lumberyard->scale_anim_duration);
                entity_set_scale(id, current_scale);
            }
        }

        // Smoke particle configuration (easy to tweak)
        F32 constexpr particle_spawn_rate_multiplier = 1.50F; // Higher = more particles per wood count
        F32 constexpr particle_scale_base            = 5.0F; // Base particle scale
        F32 constexpr particle_scale_growth          = 0.25F; // How much scale grows with wood count
        Color constexpr smoke_color_start            = YELLOW;
        Color constexpr smoke_color_end              = MAROON;

        // Calculate particle spawn rate (frame-rate independent)
        // Uses sqrt curve: wood=0 -> 0 particles/sec, wood=100 -> ~0.5 particles/sec
        F32 const wood_factor          = math_sqrt_f32((F32)lumberyard->wood_count);
        F32 const particles_per_second = wood_factor * particle_spawn_rate_multiplier;
        lumberyard->particle_spawn_accumulator += particles_per_second * dt;

        // Spawn accumulated particles
        SZ const particles_to_spawn = (SZ)lumberyard->particle_spawn_accumulator;
        if (particles_to_spawn > 0) {
            lumberyard->particle_spawn_accumulator -= (F32)particles_to_spawn;

            // Calculate spawn position at building peak
            Vector3 const lumberyard_pos   = g_world->position[id];
            Vector3 const lumberyard_scale = g_world->scale[id];
            BoundingBox const model_bbox   = asset_get_model_by_hash(g_world->model_name_hash[id])->bb;
            F32 const scaled_height        = (model_bbox.max.y - model_bbox.min.y) * lumberyard_scale.y;
            Vector3 const peak_pos         = {lumberyard_pos.x, lumberyard_pos.y + scaled_height, lumberyard_pos.z};

            // Calculate particle scale (cube root for very slow growth: wood=1000 -> 2x scale)
            F32 const scale_multiplier = particle_scale_base + (math_pow_f32((F32)lumberyard->wood_count, 1.0F / 3.0F) * particle_scale_growth);

            particles3d_add_effect_smoke(peak_pos, g_world->radius[id], smoke_color_start, smoke_color_end, scale_multiplier, particles_to_spawn);
        }
    } break;
    default: {
        _unimplemented_();
    }
    }
}
