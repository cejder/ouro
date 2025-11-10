#include "entity_actor.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "audio.hpp"
#include "color.hpp"
#include "common.hpp"
#include "cvar.hpp"
#include "dungeon.hpp"
#include "entity.hpp"
#include "entity_spawn.hpp"
#include "log.hpp"
#include "math.hpp"
#include "message.hpp"
#include "particles_3d.hpp"
#include "profiler.hpp"
#include "render.hpp"
#include "std.hpp"
#include "string.hpp"
#include "time.hpp"
#include "world.hpp"

#include <raymath.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

C8 const static *i_behavior_state_strings[ENTITY_BEHAVIOR_STATE_COUNT] = {
    "IDLE",
    "GOING TO TARGET",
    "HARVESTING TARGET",
    "DELIVERING TO LUMBERYARD",
    "ATTACKING NPC",
};

C8 const static *i_movement_state_strings[ENTITY_MOVEMENT_STATE_COUNT] = {
    "MOVEMENT_IDLE",
    "MOVEMENT_MOVING",
    "MOVEMENT_TURNING",
    "MOVEMENT_STOPPING",
};

C8 const static *i_movement_goal_strings[ENTITY_MOVEMENT_GOAL_COUNT] = {
    "NONE",
    "MOVE_TO_POS",
    "TURN_TO_ANGLE",
    "TURN_TO_TARGET",
};

BOOL static inline i_is_target_valid(EID target_id, U32 target_gen) {
    if (target_id == INVALID_EID)                                        { return false; }
    if (!ENTITY_HAS_FLAG(g_world->flags[target_id], ENTITY_FLAG_IN_USE)) { return false; }
    return g_world->generation[target_id] == target_gen;
}

BOOL static inline i_actor_is_full(EID id) {
    return g_world->actor[id].behavior.wood_count >= ACTOR_WOOD_COLLECTED_MAX;
}

void static inline i_play_agree(EID id) {
    F32 const time = time_get_glfw();
    if (time < 5.0F) { return; }

    S32 static last_time_index = 0;
    S32 rand_index             = 0;
    do { rand_index            = random_s32(0, 7); } while (rand_index == last_time_index);
    last_time_index            = rand_index;

    audio_set_pitch(ACG_VOICE, random_f32(1.25F, 1.75F));
    audio_play_3d_at_entity(ACG_VOICE, TS("agree_%d.ogg", rand_index)->c, id);
}

EID static inline i_find_target(EID searcher_id, EntityType target_type) {
    Vector3 const searcher_pos = g_world->position[searcher_id];
    F32 search_radius          = 250.0F;
    S32 attempts               = 0;
    S32 const max_attempts     = 4;

    if (target_type == ENTITY_TYPE_VEGETATION) { world_update_follower_cache(); }

    EID best_candidate            = INVALID_EID;
    F32 best_distance_sq          = F32_MAX;
    Vector2 const searcher_coords = grid_world_to_grid_coords(searcher_pos);
    S32 const searcher_x          = (S32)searcher_coords.x;
    S32 const searcher_y          = (S32)searcher_coords.y;

    while (attempts < max_attempts && search_radius <= 500 * 2.0F) {
        S32 const cell_radius      = (S32)(search_radius * g_grid.inv_cell_size) + 1;
        S32 const cell_radius_sq   = cell_radius * cell_radius;
        F32 const search_radius_sq = search_radius * search_radius;

        // Clamp dy range to valid grid bounds
        S32 const dy_min = glm::max(-cell_radius, -searcher_y);
        S32 const dy_max = glm::min(cell_radius, GRID_CELLS_PER_ROW - 1 - searcher_y);

        for (S32 dy = dy_min; dy <= dy_max; ++dy) {
            S32 const cell_y = searcher_y + dy;
            S32 const dy_sq = dy * dy;

            // Clamp dx range to valid grid bounds
            S32 const dx_min = glm::max(-cell_radius, -searcher_x);
            S32 const dx_max = glm::min(cell_radius, GRID_CELLS_PER_ROW - 1 - searcher_x);

            for (S32 dx = dx_min; dx <= dx_max; ++dx) {
                if ((dx * dx) + dy_sq > cell_radius_sq) { continue; }

                S32 const cell_x = searcher_x + dx;

                SZ const cell_index = grid_get_cell_index_xy(cell_x, cell_y);
                GridCell *cell      = &g_grid.cells[cell_index];

                SZ const count = cell->count_per_type[target_type];
                if (count == 0) { continue; }

                for (SZ i = 0; i < count; ++i) {
                    EID const entity_id = cell->entities_by_type[target_type][i];
                    if (entity_id == searcher_id) { continue; }

                    // Validate entity is still valid and has correct type (grid may have stale entries)
                    if (!entity_is_valid(entity_id)) { continue; }
                    if (g_world->type[entity_id] != target_type) { continue; }

                    if (target_type == ENTITY_TYPE_VEGETATION) {
                        S32 const followers = (S32)g_world->follower_cache.follower_counts[entity_id];
                        if (followers >= HARVEST_TARGET_MAX_FOLLOWERS) { continue; }
                    }

                    F32 const distance_sqr = Vector3DistanceSqr(searcher_pos, g_world->position[entity_id]);
                    if (distance_sqr > search_radius_sq || distance_sqr >= best_distance_sq) { continue; }

                    best_candidate   = entity_id;
                    best_distance_sq = distance_sqr;
                }
            }
        }

        search_radius *= 2.0F;
        attempts++;
    }

    return best_candidate;
}

BOOL static inline i_has_reached_target(EID actor_id, Vector3 target_position, EID target_id) {
    Vector3 const actor_position = g_world->position[actor_id];
    F32 const actor_radius       = g_world->radius[actor_id];
    F32 dist_sq                  = Vector3DistanceSqr(actor_position, target_position); // Compute distance squared once
    F32 threshold_sq             = actor_radius * actor_radius;                         // Default threshold for invalid target

    if (entity_is_valid(target_id)) {
        // NOLINTNEXTLINE(clang-analyzer-security.ArrayBound) - entity_is_valid() guards against INVALID_EID
        target_position            = g_world->position[target_id];
        dist_sq                    = Vector3DistanceSqr(actor_position, target_position);
        F32 const target_radius    = g_world->radius[target_id];
        F32 const base_tolerance   = g_world->type[target_id] == ENTITY_TYPE_BUILDING_LUMBERYARD ? 1.75F : 1.25F;
        F32 const stuck_factor     = glm::max(1.0F, g_world->actor[actor_id].movement.stuck_timer);
        F32 const tolerance        = base_tolerance * stuck_factor;
        F32 const interaction_dist = (actor_radius + target_radius) * tolerance;
        threshold_sq               = interaction_dist * interaction_dist;
    }

    return dist_sq <= threshold_sq;
}

Vector3 static inline i_calculate_separation_force(EID id) {
    Vector3 separation        = {};
    Vector3 const current_pos = g_world->position[id];

    // Extract components once
    F32 const current_x = current_pos.x;
    F32 const current_z = current_pos.z;

    Vector2 const grid_coords = grid_world_to_grid_coords(current_pos);
    S32 const center_x        = (S32)grid_coords.x;
    S32 const center_y        = (S32)grid_coords.y;

    // Get current entity's precalculated radius
    F32 const current_radius = g_world->radius[id];

    // Precomputed constants
    F32 const min_sqr = 0.01F;  // 0.1^2

    // Clamp bounds once instead of checking in loop
    S32 const min_y = glm::max(center_y - 1, 0);
    S32 const max_y = glm::min(center_y + 1, GRID_CELLS_PER_ROW - 1);
    S32 const min_x = glm::max(center_x - 1, 0);
    S32 const max_x = glm::min(center_x + 1, GRID_CELLS_PER_ROW - 1);

    EntityType static constexpr relevant_types[] = {
        ENTITY_TYPE_NPC,
        ENTITY_TYPE_BUILDING_LUMBERYARD,
        ENTITY_TYPE_VEGETATION,
    };

    // Process 3x3 neighborhood
    for (S32 cell_y = min_y; cell_y <= max_y; ++cell_y) {
        for (S32 cell_x = min_x; cell_x <= max_x; ++cell_x) {

            SZ const cell_index = grid_get_cell_index_xy(cell_x, cell_y);
            GridCell *cell = &g_grid.cells[cell_index];

            // Process relevant entity types directly
            for (EntityType other_type : relevant_types) {
                SZ const count = cell->count_per_type[other_type];
                if (count == 0) { continue; }

                for (SZ i = 0; i < count; ++i) {
                    EID const other_id = cell->entities_by_type[other_type][i];
                    if (other_id == id) { continue; }

                    // Calculate separation distance based on both entities' actual sizes
                    F32 const separation_distance   = current_radius + g_world->radius[other_id];
                    F32 const separation_radius_sqr = separation_distance * separation_distance;

                    // Calculate full 3D difference
                    F32 const diff_x = current_x - g_world->position[other_id].x;
                    F32 const diff_z = current_z - g_world->position[other_id].z;

                    // Calculate squared length of modified vector
                    F32 const distance_sqr = (diff_x * diff_x) + (diff_z * diff_z);

                    if (distance_sqr > min_sqr && distance_sqr < separation_radius_sqr) {
                        F32 const inv_separation_radius_sqr = 1.0F / separation_radius_sqr;
                        F32 const strength_sqr              = (separation_radius_sqr - distance_sqr) * inv_separation_radius_sqr;

                        // Square root for normalization
                        F32 const inv_distance = glm::inversesqrt(distance_sqr);
                        separation.x += diff_x * inv_distance * strength_sqr;
                        separation.z += diff_z * inv_distance * strength_sqr;
                    }
                }
            }
        }
    }

    // Once done, one more time with the player:

    // Separation from player
    // NOTE: We want a more pronounced reaction here, people don't stop right before you hitting you.
    Vector3 const player_position = g_player.cameras[g_scenes.current_scene_type].position;
    F32 const player_radius       = PLAYER_RADIUS * 2.0F;

    // Calculate separation distance based on both entity's and player's actual sizes
    F32 const player_separation_distance   = current_radius + player_radius;
    F32 const player_separation_radius_sqr = player_separation_distance * player_separation_distance;

    // Calculate full 3D difference with player
    F32 const player_diff_x = current_x - player_position.x;
    F32 const player_diff_z = current_z - player_position.z;

    // Calculate squared length of vector to player
    F32 const player_distance_sqr = (player_diff_x * player_diff_x) + (player_diff_z * player_diff_z);

    if (player_distance_sqr > min_sqr && player_distance_sqr < player_separation_radius_sqr) {
        F32 const inv_player_separation_radius_sqr = 1.0F / player_separation_radius_sqr;
        F32 const player_strength_sqr = (player_separation_radius_sqr - player_distance_sqr) * inv_player_separation_radius_sqr;

        // Square root for normalization
        F32 const inv_player_distance = glm::inversesqrt(player_distance_sqr);
        separation.x += player_diff_x * inv_player_distance * player_strength_sqr;
        separation.z += player_diff_z * inv_player_distance * player_strength_sqr;

        // Occasional audio feedback - check time only once per second at most
        static F32 last_time = 0.0F;
        static U32 frame_counter = 0;

        // Only check time every 60 frames (~1 second at 60fps) to avoid expensive time_get() calls
        if (++frame_counter >= 60) {
            frame_counter = 0;
            F32 const this_time = time_get();

            if (this_time - last_time >= 5.0F) {
                if (random_s32(0, 1)) {
                    audio_set_pitch(ACG_SFX, random_f32(1.5F, 2.0F));
                    audio_play_3d_at_position(ACG_SFX, TS("agree_%d.ogg", random_s32(0, 7))->c, current_pos);
                } else {
                    audio_play_3d_at_position(ACG_SFX, TS("cute_%d.ogg", random_s32(0, 6))->c, current_pos);
                }

                last_time = this_time;
            }
        }
    }

    return separation;
}

Vector3 static inline i_calculate_offset_target_position(EID id, Vector3 base_target) {
    EntityMovementController *movement = &g_world->actor[id].movement;

    if (!movement->use_target_offset) {
        movement->target_offset_angle = ((F32)(id % 16) / 16.0F) * 2.0F * glm::pi<F32>();
        movement->use_target_offset = true;
    }

    F32 const offset_radius = 3.0F;
    F32 const offset_x      = math_cos_f32(movement->target_offset_angle) * offset_radius;
    F32 const offset_z      = math_sin_f32(movement->target_offset_angle) * offset_radius;

    Vector3 offset_target = base_target;
    offset_target.x      += offset_x;
    offset_target.z      += offset_z;
    // NOTE: I commented this out, I think we do not need this.
    // offset_target.y       = math_get_terrain_height(g_world->base_terrain, offset_target.x, offset_target.z);

    return offset_target;
}

U32 static inline i_count_harvesters_for_target(EID target_id, F32 *lowest_timer_out) {
    Vector3 const target_pos = g_world->position[target_id];

    Vector2 const grid_coords = grid_world_to_grid_coords(target_pos);
    S32 const center_x        = (S32)grid_coords.x;
    S32 const center_y        = (S32)grid_coords.y;

    U32 harvesters_count = 0;
    F32 lowest_timer = F32_MAX;

    for (S32 dy = -1; dy <= 1; ++dy) {
        for (S32 dx = -1; dx <= 1; ++dx) {
            S32 const cell_x = center_x + dx;
            S32 const cell_y = center_y + dy;

            if (cell_x < 0 || cell_x >= GRID_CELLS_PER_ROW || cell_y < 0 || cell_y >= GRID_CELLS_PER_ROW) { continue; }

            SZ const cell_index = grid_get_cell_index_xy(cell_x, cell_y);
            GridCell *cell      = &g_grid.cells[cell_index];

            SZ const count = cell->count_per_type[ENTITY_TYPE_NPC];
            for (SZ i = 0; i < count; ++i) {
                EID const entity_id = cell->entities_by_type[ENTITY_TYPE_NPC][i];

                if (!ENTITY_HAS_FLAG(g_world->flags[entity_id], ENTITY_FLAG_IN_USE)) { continue; }

                EntityBehaviorController const *other_behavior = &g_world->actor[entity_id].behavior;
                if (other_behavior->state == ENTITY_BEHAVIOR_STATE_HARVESTING_TARGET &&
                    i_is_target_valid(other_behavior->target_id, other_behavior->target_gen) && other_behavior->target_id == target_id) {
                    harvesters_count++;
                    lowest_timer = glm::min(other_behavior->action_timer, lowest_timer);
                }
            }
        }
    }

    *lowest_timer_out = lowest_timer;
    return harvesters_count;
}

void static inline i_movement_transition_to_state(EID id, EntityMovementState new_state) {
    EntityMovementController *movement  = &g_world->actor[id].movement;
    EntityMovementState const old_state = movement->state;

    if (old_state == new_state) { return; }

    // Handle movement state enter

    switch (new_state) {
        case ENTITY_MOVEMENT_STATE_IDLE: {
            movement->current_speed  = 0.0F;
            movement->velocity       = {0, 0, 0};
            movement->goal_completed = true;
        } break;

        case ENTITY_MOVEMENT_STATE_MOVING: {
            movement->goal_completed = false;
        } break;

        case ENTITY_MOVEMENT_STATE_TURNING: {
            movement->turn_start_angle = g_world->rotation[id];

            F32 const target_angle      = math_normalize_angle_0_360(movement->target_angle);
            F32 const delta_angle       = math_normalize_angle_delta(target_angle - movement->turn_start_angle);
            movement->turn_target_angle = movement->turn_start_angle + delta_angle;

            F32 const base_duration = 0.4F;
            F32 const max_duration  = 0.8F;
            F32 const angle_factor  = math_abs_f32(delta_angle) / 180.0F;
            movement->turn_duration = base_duration + ((max_duration - base_duration) * math_sqrt_f32(angle_factor));
            movement->turn_elapsed  = 0.0F;
        } break;

        default: {
        } break;
    }

    if (c_world__verbose_actors) {
        lln("\\ouc{#00ffaaff}[%04d] "
            "\\ouc{#aaaaaa88}MOVEMENT: "
            "\\ouc{#ff6b6b88}%-18s "
            "\\ouc{#ffff0088}-> "
            "\\ouc{#4ecdc488}%-18s",
            id, i_movement_state_strings[old_state], i_movement_state_strings[new_state]);
    }

    movement->state = new_state;
}

void static inline i_movement_set_goal_move_to_position(EID id, Vector3 position) {
    EntityMovementController *movement = &g_world->actor[id].movement;

    movement->goal_type       = ENTITY_MOVEMENT_GOAL_MOVE_TO_POSITION;
    movement->target_position = position;
    movement->target_id       = INVALID_EID;
    movement->target_gen      = 0;
    movement->goal_completed  = false;
    movement->goal_failed     = false;

    i_movement_transition_to_state(id, ENTITY_MOVEMENT_STATE_MOVING);
}

void static inline i_movement_set_goal_move_to_target(EID id, EID target_id, U32 target_gen) {
    EntityMovementController *movement = &g_world->actor[id].movement;

    movement->goal_type      = ENTITY_MOVEMENT_GOAL_MOVE_TO_POSITION;
    movement->target_id      = target_id;
    movement->target_gen     = target_gen;
    movement->goal_completed = false;
    movement->goal_failed    = false;

    if (i_is_target_valid(target_id, target_gen)) {
        movement->target_position = g_world->position[target_id];
        i_movement_transition_to_state(id, ENTITY_MOVEMENT_STATE_MOVING);
    } else {
        movement->goal_failed = true;
    }
}

void static inline i_movement_set_goal_turn_to_angle(EID id, F32 angle, BOOL use_easing, EaseType ease_type) {
    EntityMovementController *movement = &g_world->actor[id].movement;

    movement->goal_type      = ENTITY_MOVEMENT_GOAL_TURN_TO_ANGLE;
    movement->target_angle   = angle;
    movement->turn_ease_type = ease_type;
    movement->goal_completed = false;
    movement->goal_failed    = false;

    if (use_easing) {
        i_movement_transition_to_state(id, ENTITY_MOVEMENT_STATE_TURNING);
    } else {
        entity_set_rotation(id, angle);
        movement->goal_completed = true;
    }
}

void static inline i_movement_set_goal_turn_to_target(EID id, Vector3 target_position, BOOL use_easing, EaseType ease_type) {
    Vector3 const current_pos = g_world->position[id];
    Vector3 const direction   = Vector3Subtract(target_position, current_pos);
    F32 const angle           = (-math_atan2_f32(direction.z, direction.x) * RAD2DEG) + 90.0F;

    i_movement_set_goal_turn_to_angle(id, angle, use_easing, ease_type);
}

void static inline i_movement_set_goal_stop(EID id) {
    EntityMovementController *movement = &g_world->actor[id].movement;

    movement->goal_type      = ENTITY_MOVEMENT_GOAL_STOP;
    movement->goal_completed = false;
    movement->goal_failed    = false;

    i_movement_transition_to_state(id, ENTITY_MOVEMENT_STATE_STOPPING);
}

BOOL static inline i_movement_is_goal_completed(EID id) {
    return g_world->actor[id].movement.goal_completed;
}

BOOL static inline i_movement_is_goal_failed(EID id) {
    return g_world->actor[id].movement.goal_failed;
}

void static inline i_evaluate_behavior_transitions(EID id) {
    EntityBehaviorController *behavior       = &g_world->actor[id].behavior;
    EntityMovementController const *movement = &g_world->actor[id].movement;

    // Check global conditions first
    if (!i_is_target_valid(behavior->target_id, behavior->target_gen) && behavior->target_id != INVALID_EID) {
        // Target was destroyed
        entity_actor_clear_actor_target(id);
    }

    // State-specific transition logic
    switch (behavior->state) {
        case ENTITY_BEHAVIOR_STATE_IDLE: {
            // Idle doesn't transition on its own - only external commands
        } break;

        case ENTITY_BEHAVIOR_STATE_GOING_TO_TARGET: {
            if (movement->goal_failed) {
                entity_actor_search_and_transition_to_target(id, "Movement goal failed");
            } else if (movement->goal_completed) {
                // Decide what to do based on target type
                if (behavior->target_entity_type == ENTITY_TYPE_BUILDING_LUMBERYARD) {
                    if (behavior->wood_count > 0) {
                        entity_actor_behavior_transition_to_state(id, ENTITY_BEHAVIOR_STATE_DELIVERING_TO_LUMBERYARD, "Reached lumberyard with wood");
                    } else {
                        behavior->target_entity_type = ENTITY_TYPE_VEGETATION;
                        entity_actor_search_and_transition_to_target(id, "Reached lumberyard but have no wood");
                    }
                } else if (behavior->target_entity_type == ENTITY_TYPE_VEGETATION) {
                    entity_actor_behavior_transition_to_state(id, ENTITY_BEHAVIOR_STATE_HARVESTING_TARGET, "Reached vegetation and starting harvest");
                } else if (behavior->target_entity_type == ENTITY_TYPE_NPC && behavior->target_id != INVALID_EID) {
                    entity_actor_behavior_transition_to_state(id, ENTITY_BEHAVIOR_STATE_ATTACKING_NPC, "Reached attack target and starting combat");
                } else {
                    entity_actor_behavior_transition_to_state(id, ENTITY_BEHAVIOR_STATE_IDLE, "Reached target and going idle");
                }
            }
        } break;

        case ENTITY_BEHAVIOR_STATE_HARVESTING_TARGET: {
            if (i_actor_is_full(id)) {
                // Check if full
                behavior->target_entity_type = ENTITY_TYPE_BUILDING_LUMBERYARD;
                entity_actor_search_and_transition_to_target(id, TS("Became full (%zu/%d)", behavior->wood_count, ACTOR_WOOD_COLLECTED_MAX)->c);
            } else if (!i_is_target_valid(behavior->target_id, behavior->target_gen)) {
                // Check if gone
                entity_actor_search_and_transition_to_target(id, "Target disappeared");
            } else if (behavior->action_timer <= 0.0F) {
                // Harvest complete
                if (c_world__verbose_actors) {
                    lln("\\ouc{#00ffaaff}[%04d] "
                        "\\ouc{#ffcdc4ff}HARVEST COMPLETE "
                        "\\ouc{#ffffffff}%s:%04d ",
                        id, entity_type_to_cstr(behavior->target_entity_type), behavior->target_id);
                }

                // Final destruction burst when entity is actually destroyed
                Vector3 const target_pos = g_world->position[behavior->target_id];
                Color const actor_color  = color_saturated(g_world->tint[id]);
                particles3d_add_effect_harvest_complete(target_pos, actor_color, Fade(color_darken(actor_color, 0.3F), 0.0F), HARVEST_COMPLETE_SIZE_MULTIPLIER, HARVEST_COMPLETE_PARTICLE_COUNT);
                audio_play_3d_at_position(ACG_SFX, "plop.ogg", g_world->position[behavior->target_id]);
                entity_destroy(behavior->target_id);
                behavior->wood_count++;

                if (i_actor_is_full(id)) { behavior->target_entity_type = ENTITY_TYPE_BUILDING_LUMBERYARD; }

                world_notify_actors_target_destroyed(behavior->target_id);

                entity_actor_search_and_transition_to_target(id, TS("Harvested tree, wood: %zu/%d", behavior->wood_count, ACTOR_WOOD_COLLECTED_MAX)->c);

                // TODO: Remove this later
                entity_spawn_random_vegetation_on_terrain(g_world->base_terrain, 1, false);
            }
        } break;

        case ENTITY_BEHAVIOR_STATE_DELIVERING_TO_LUMBERYARD: {
            if (!i_is_target_valid(behavior->target_id, behavior->target_gen)) {
                // Check if gone
                entity_actor_search_and_transition_to_target(id, "Target disappeared");
            } else if (behavior->action_timer <= 0.0F) {
                // Delivery complete
                SZ const wood_delivered = behavior->wood_count;

                g_world->building[behavior->target_id].lumberyard.wood_count += wood_delivered;

                // Scale lumberyard by wood count with easing animation
                EID const lumberyard_id         = behavior->target_id;
                SZ const total_wood             = g_world->building[lumberyard_id].lumberyard.wood_count;
                F32 const growth_per_wood       = 0.05F;
                F32 const max_horizontal_scale  = 1.0F;
                F32 const vertical_bonus_factor = 1.5F;

                F32 const scale_factor = 1.0F + ((F32)total_wood * growth_per_wood);
                Vector3 const original = g_world->original_scale[lumberyard_id];
                Vector3 new_scale;

                if (scale_factor <= max_horizontal_scale) {
                    new_scale = Vector3Scale(original, scale_factor);
                } else {
                    SZ const wood_for_max_horizontal = (SZ)((max_horizontal_scale - 1.0F) / growth_per_wood);
                    SZ const excess_wood             = total_wood - wood_for_max_horizontal;
                    F32 const effective_excess_wood  = (F32)excess_wood * vertical_bonus_factor;
                    F32 const vertical_scale_factor  = max_horizontal_scale + (effective_excess_wood * growth_per_wood);

                    new_scale.x = original.x * max_horizontal_scale;
                    new_scale.z = original.z * max_horizontal_scale;
                    new_scale.y = original.y * vertical_scale_factor;
                }

                // Start/update the easing animation
                EntityBuildingLumberyard *lumberyard = &g_world->building[lumberyard_id].lumberyard;

                // If already animating, restart from current position to avoid jumping
                if (lumberyard->is_animating_scale) {
                    // Restart animation from current scale (not original start_scale)
                    lumberyard->start_scale         = g_world->scale[lumberyard_id];
                    lumberyard->target_scale        = new_scale;
                    lumberyard->scale_anim_elapsed  = 0.0F;
                    lumberyard->scale_anim_duration = 0.8F;
                } else {
                    // Start fresh animation
                    lumberyard->start_scale         = g_world->scale[lumberyard_id];
                    lumberyard->target_scale        = new_scale;
                    lumberyard->scale_anim_elapsed  = 0.0F;
                    lumberyard->scale_anim_duration = 0.8F;
                    lumberyard->is_animating_scale  = true;
                }

                if (c_world__verbose_actors) {
                    lln("\\ouc{#00ffaaff}[%04d] "
                        "\\ouc{#900a90ff}DELIVERY COMPLETE "
                        "\\ouc{#ffffffff}lumberyard:%04d "
                        "\\ouc{#ff9500ff}wood:%zu",
                        id, behavior->target_id, wood_delivered);
                }

                behavior->wood_count = 0;
                audio_play_3d_at_position(ACG_SFX, "mario_coin.ogg", g_world->position[behavior->target_id]);

                // Clear the old target (lumberyard) before searching for new target
                entity_actor_clear_actor_target(id);

                behavior->target_entity_type = ENTITY_TYPE_VEGETATION;
                entity_actor_search_and_transition_to_target(id, TS("Delivered %zu wood", wood_delivered)->c);
            }
        } break;

        case ENTITY_BEHAVIOR_STATE_ATTACKING_NPC: {
            if (!i_is_target_valid(behavior->target_id, behavior->target_gen)) {
                entity_actor_behavior_transition_to_state(id, ENTITY_BEHAVIOR_STATE_IDLE, "Attack target disappeared");
            } else {
                // Check range
                Vector3 const attacker_pos = g_world->position[id];
                Vector3 const target_pos   = g_world->position[behavior->target_id];
                F32 const distance         = Vector3Distance(attacker_pos, target_pos);

                if (distance > ATTACK_RANGE_EXIT) {
                    entity_actor_behavior_transition_to_state(id, ENTITY_BEHAVIOR_STATE_GOING_TO_TARGET, "Target moved out of attack range");
                } else if (behavior->action_timer <= 0.0F) {
                    // Attack complete
                    audio_play_3d_at_position(ACG_SFX, "hit.ogg", g_world->position[behavior->target_id]);
                    behavior->action_timer = asset_get_animation_duration_by_hash(g_world->model_name_hash[id], 3, g_world->animation[id].anim_fps, CESIUM_ANIMATION_SPEED);

                    BOOL const is_dead =  entity_damage(behavior->target_id, ATTACK_DAMAGE);

                    if (c_world__verbose_actors) {
                        lln("\\ouc{#ffff00ff}[%04d] ATTACK HIT on target %04d %s(%s)", id, behavior->target_id,
                            is_dead ? "\\ouc{#ff0000ff}" : "\\ouc{#00ff00ff}", is_dead ? "DEAD" : "ALIVE");
                    }

                    if (is_dead) {
                        world_notify_actors_target_destroyed(behavior->target_id);

                        entity_actor_behavior_transition_to_state(id, ENTITY_BEHAVIOR_STATE_IDLE, TS("Target (%u) is dead, starting to idle", behavior->target_id)->c);
                    }
                }
            }
        } break;

        default: {
            _unreachable_();
        }
    }
}

void entity_actor_init() {
    g_world->follower_cache.dirty = true;
    world_update_follower_cache();
}

void entity_actor_update(EID id, F32 dt) {
    EntityActor *actor                 = &g_world->actor[id];
    EntityBehaviorController *behavior = &actor->behavior;
    EntityMovementController *movement = &actor->movement;

    // First evaluate if any state transitions are needed
    i_evaluate_behavior_transitions(id);

    // Then update the current states
    switch (movement->state) {
        case ENTITY_MOVEMENT_STATE_IDLE: {
            // Apply gentle repulsion if entities are overlapping while idle
            F32 constexpr idle_repulsion_strength = 0.25F;  // Much weaker than moving separation
            Vector3 const separation = i_calculate_separation_force(id);

            if (Vector3LengthSqr(separation) > 0.001F) {  // Only apply if there's significant overlap
                Vector3 const repulsion_velocity = Vector3Scale(separation, idle_repulsion_strength);
                Vector3 new_position = Vector3Add(g_world->position[id], Vector3Scale(repulsion_velocity, dt));
                new_position.y = math_get_terrain_height(g_world->base_terrain, new_position.x, new_position.z);

                // Resolve dungeon wall collision with sliding if in dungeon scene
                if (g_scenes.current_scene_type == SCENE_DUNGEON) {
                    new_position = dungeon_resolve_wall_collision(id, new_position);
                }

                entity_set_position(id, new_position);
            }

            switch (movement->goal_type) {
                case ENTITY_MOVEMENT_GOAL_MOVE_TO_POSITION: {
                    i_movement_transition_to_state(id, ENTITY_MOVEMENT_STATE_MOVING);
                } break;
                case ENTITY_MOVEMENT_GOAL_TURN_TO_ANGLE: {
                    i_movement_transition_to_state(id, ENTITY_MOVEMENT_STATE_TURNING);
                } break;
                case ENTITY_MOVEMENT_GOAL_STOP: {
                    movement->goal_completed = true;
                } break;
                default: {
                    break;
                }
            }
        } break;

        case ENTITY_MOVEMENT_STATE_MOVING: {
            // Update target position if following an entity
            if (i_is_target_valid(movement->target_id, movement->target_gen)) {
                Vector3 const base_target = g_world->position[movement->target_id];
                movement->target_position = i_calculate_offset_target_position(id, base_target);
            } else if (movement->target_id != INVALID_EID) {
                movement->goal_failed = true;
                i_movement_transition_to_state(id, ENTITY_MOVEMENT_STATE_IDLE);
                return;
            }

            Vector3 const current_pos = g_world->position[id];
            Vector3 const direction   = Vector3Subtract(movement->target_position, current_pos);

            // Check if we've reached the target
            if (i_has_reached_target(id, movement->target_position, movement->target_id)) {
                movement->goal_completed = true;
                i_movement_transition_to_state(id, ENTITY_MOVEMENT_STATE_STOPPING);
                return;
            }

            // Calculate separation force
            Vector3 separation         = i_calculate_separation_force(id);
            movement->separation_force = separation;

            // Handle stuck detection
            Vector3 const actual_velocity = Vector3Scale(Vector3Subtract(current_pos, movement->last_position), 1.0F / dt);

            if (Vector3LengthSqr(actual_velocity) < 0.1F) {
                movement->stuck_timer += dt;
                if (movement->stuck_timer > ACTOR_MAX_STUCK_TIME) {
                    F32 const escape_angle     = random_f32(0.0F, 2.0F * glm::pi<F32>());
                    Vector3 const escape_force = {math_cos_f32(escape_angle) * 3.0F, 0, math_sin_f32(escape_angle) * 3.0F};
                    separation = Vector3Add(separation, escape_force);
                    movement->stuck_timer = 0.0F;
                }
            } else {
                movement->stuck_timer = 0.0F;
            }
            movement->last_position = current_pos;

            // Calculate desired movement direction
            Vector3 const desired_direction = Vector3Normalize(direction);
            F32 target_speed                = movement->speed;

            // Apply separation force
            Vector3 final_direction = Vector3Add(desired_direction, Vector3Scale(separation, 1.5F));

            // Limit direction magnitude
            F32 const direction_magnitude_sqr = Vector3LengthSqr(final_direction);
            if (direction_magnitude_sqr > 1.0F) { final_direction = Vector3Normalize(final_direction); }

            // Dynamic speed based on distance to target
            F32 const distance_to_target_sqr = Vector3LengthSqr(direction);
            if (distance_to_target_sqr < 25.0F) {  // 5.0F * 5.0F
                F32 const distance_to_target = math_sqrt_f32(distance_to_target_sqr);
                target_speed                *= (distance_to_target / 5.0F);
                target_speed                 = glm::max(target_speed, movement->speed * 0.3F);
            }

            // Calculate desired velocity
            Vector3 const desired_velocity = Vector3Scale(final_direction, target_speed);

            // Apply movement
            Vector3 new_position = Vector3Add(g_world->position[id], Vector3Scale(desired_velocity, dt));
            new_position.y       = math_get_terrain_height(g_world->base_terrain, new_position.x, new_position.z);

            // Resolve dungeon wall collision with sliding if in dungeon scene
            if (g_scenes.current_scene_type == SCENE_DUNGEON) {
                new_position = dungeon_resolve_wall_collision(id, new_position);
            }

            entity_set_position(id, new_position);

            movement->velocity = desired_velocity;

            // Handle turning - Always face the direction we're actually moving
            // When separation forces are active, we naturally face where we're going
            // When no separation, we face the target - both cases look natural
            F32 const separation_strength = Vector3Length(separation);
            BOOL const has_separation     = separation_strength > 0.01F;

            // If we have meaningful separation forces, face the actual movement direction
            // Otherwise, face directly toward the target for cleaner movement
            Vector3 const facing_direction = has_separation ? final_direction : desired_direction;
            F32 const target_rotation      = (-math_atan2_f32(facing_direction.z, facing_direction.x) * RAD2DEG) + 90.0F;
            F32 current_rotation           = g_world->rotation[id];
            current_rotation               = math_normalize_angle_0_360(current_rotation);
            F32 normalized_target          = math_normalize_angle_0_360(target_rotation);
            F32 const delta_angle          = math_normalize_angle_delta(normalized_target - current_rotation);
            F32 const abs_delta            = math_abs_f32(delta_angle);

            // Only apply rotation if the angle difference is meaningful (> 1 degree)
            // This prevents jittering while still correcting persistent angle errors
            if (abs_delta > 1.0F) {
                F32 const turn_speed          = 480.0F;  // Faster turn speed for more responsive movement
                F32 const max_turn_this_frame = turn_speed * dt;
                F32 turn_amount               = delta_angle;
                if (abs_delta > max_turn_this_frame) { turn_amount = (turn_amount > 0.0F) ? max_turn_this_frame : -max_turn_this_frame; }
                entity_set_rotation(id, current_rotation + turn_amount);
            }
        } break;

        case ENTITY_MOVEMENT_STATE_TURNING: {
            movement->turn_elapsed += dt;

            if (movement->turn_elapsed >= movement->turn_duration) {
                entity_set_rotation(id, movement->turn_target_angle);
                movement->goal_completed = true;
                i_movement_transition_to_state(id, ENTITY_MOVEMENT_STATE_IDLE);
            } else {
                F32 const current_angle = ease_use(movement->turn_ease_type,
                                                   movement->turn_elapsed,
                                                   movement->turn_start_angle,
                                                   movement->turn_target_angle - movement->turn_start_angle,
                                                   movement->turn_duration);
                entity_set_rotation(id, current_angle);
            }
        } break;

        case ENTITY_MOVEMENT_STATE_STOPPING: {
            if (movement->current_speed <= 0.01F) {
                movement->current_speed  = 0.0F;
                movement->velocity       = {0, 0, 0};
                movement->goal_completed = true;
                i_movement_transition_to_state(id, ENTITY_MOVEMENT_STATE_IDLE);
                return;
            }

            movement->current_speed = glm::max(0.0F, movement->current_speed - (movement->acceleration * dt * 3.0F));

            if (Vector3Length(movement->velocity) > 0.01F) {
                Vector3 const normalized_velocity = Vector3Normalize(movement->velocity);
                Vector3 new_position = Vector3Add(g_world->position[id], Vector3Scale(normalized_velocity, movement->current_speed * dt));
                new_position.y = math_get_terrain_height(g_world->base_terrain, new_position.x, new_position.z);
                entity_set_position(id, new_position);
            }
        } break;

        default: {
            break;
        }
    }

    switch (actor->behavior.state) {
        case ENTITY_BEHAVIOR_STATE_HARVESTING_TARGET: {
            // Only do the harvesting work if target still valid
            if (i_is_target_valid(behavior->target_id, behavior->target_gen)) {
                EID const target_id        = behavior->target_id;
                F32 lowest_timer           = F32_MAX;
                U32 const harvesters_count = i_count_harvesters_for_target(target_id, &lowest_timer);

                if (behavior->action_timer == lowest_timer) {
                    F32 const progress      = 1.0F - (behavior->action_timer / ACTION_DURATION_HARVEST);
                    // Use expo ease-in to keep scale high until the very end, then drop quickly
                    F32 const eased_progress = ease_in_expo(progress, 0.0F, 1.0F, 1.0F);
                    F32 scale_factor         = 1.0F - (eased_progress * 0.99F);

                    // Add initial rustle/shake effect in the first 0.6 seconds
                    F32 const rustle_duration = 0.6F;
                    F32 const rustle_progress = glm::min(progress / (rustle_duration / ACTION_DURATION_HARVEST), 1.0F);
                    if (rustle_progress < 1.0F) {
                        F32 const shake_frequency  = 8.0F;  // Slower wobble
                        F32 const shake_amplitude  = 0.035F;  // Subtle variation
                        F32 const shake_decay      = ease_out_elastic(rustle_progress, 1.0F, -1.0F, 1.0F);  // Bouncy decay
                        F32 const shake            = math_sin_f32(progress * shake_frequency * glm::pi<F32>() * 2.0F) * shake_amplitude * shake_decay;
                        scale_factor              += shake;
                    }

                    Vector3 const new_scale = Vector3Scale(g_world->original_scale[target_id], scale_factor);
                    entity_set_scale(target_id, new_scale);

                    // Frame-rate independent particle spawning during harvest
                    Vector3 const target_pos = g_world->position[target_id];
                    Color const actor_color  = color_saturated(g_world->tint[id]);

                    F32 const spawn_interval = 1.0F / HARVEST_ACTIVE_PARTICLES_PER_SECOND;
                    behavior->particle_spawn_timer += dt;
                    while (behavior->particle_spawn_timer >= spawn_interval) {
                        particles3d_add_effect_harvest_active(target_pos, actor_color, Fade(color_darken(actor_color, 0.3F), 0.0F),
                                                             HARVEST_ACTIVE_SIZE_MULTIPLIER, HARVEST_ACTIVE_PARTICLE_COUNT);
                        behavior->particle_spawn_timer -= spawn_interval;
                    }
                }

                behavior->action_timer -= dt * (F32)harvesters_count;
            }
        } break;

        case ENTITY_BEHAVIOR_STATE_DELIVERING_TO_LUMBERYARD:
        case ENTITY_BEHAVIOR_STATE_ATTACKING_NPC: {
            behavior->action_timer -= dt;
        } break;

        default: {
        } break;
    }

    // TODO: These animation indices are hardcoded!

    // Figure out what animation we are in
    switch (actor->movement.state) {
        case ENTITY_MOVEMENT_STATE_MOVING: {
            entity_set_animation(id, 2, true, CESIUM_ANIMATION_SPEED);
        } break;

        default: {
            switch (actor->behavior.state) {
                case ENTITY_BEHAVIOR_STATE_HARVESTING_TARGET:
                case ENTITY_BEHAVIOR_STATE_ATTACKING_NPC: {
                    entity_set_animation(id, 3, true, CESIUM_ANIMATION_SPEED);
                } break;

                default: {
                    entity_set_animation(id, 1, true, CESIUM_ANIMATION_SPEED);
                } break;
            }
        } break;
    }
}

void entity_actor_behavior_transition_to_state(EID id, EntityBehaviorState new_state, C8 const *reason) {
    EntityBehaviorController *behavior  = &g_world->actor[id].behavior;
    EntityBehaviorState const old_state = behavior->state;

    _assert_(ou_strlen(reason) < ENTITY_STATE_REASON_MAX, "Reason string exceeds limit");

    ou_strncpy(behavior->state_reason, reason, ENTITY_STATE_REASON_MAX - 1);
    behavior->state_reason[ENTITY_STATE_REASON_MAX - 1] = '\0';

    // Handle behavior state exit
    switch (old_state) {
        case ENTITY_BEHAVIOR_STATE_DELIVERING_TO_LUMBERYARD: {
            audio_play_3d_at_entity(ACG_VOICE, "wah.ogg", id);
        } break;
        default: {
            break;
        }
    }

    // Handle behavior state enter
    switch (new_state) {
        case ENTITY_BEHAVIOR_STATE_IDLE: {
            entity_actor_clear_actor_target(id);
            i_movement_set_goal_stop(id);
        } break;

        case ENTITY_BEHAVIOR_STATE_GOING_TO_TARGET: {
            if (behavior->target_id != INVALID_EID) {
                i_movement_set_goal_move_to_target(id, behavior->target_id, behavior->target_gen);
            } else {
                i_movement_set_goal_move_to_position(id, behavior->target_position);
            }
        } break;

        case ENTITY_BEHAVIOR_STATE_HARVESTING_TARGET: {
            audio_set_pitch(ACG_VOICE, 1.5F);
            S32 rand = random_s32(0, 100);
            if ( rand == 0) {
                audio_play_3d_at_entity(ACG_VOICE, "chikipao.ogg", id);
            } else if (rand == 1) {
                audio_play_3d_at_entity(ACG_VOICE, "babababam.ogg", id);
            } else {
                audio_play_3d_at_entity(ACG_VOICE, TS("uhh_%d.ogg", random_s32(0, 4))->c, id);
            }

            behavior->action_timer = ACTION_DURATION_HARVEST;

            i_movement_set_goal_stop(id);

            if (i_is_target_valid(behavior->target_id, behavior->target_gen)) {
                i_movement_set_goal_turn_to_target(id, g_world->position[behavior->target_id], true, EASE_IN_OUT_CUBIC);

                // Spawn initial impact particles using actor's color
                Vector3 const target_pos = g_world->position[behavior->target_id];
                Color const actor_color  = color_saturated(g_world->tint[id]);
                particles3d_add_effect_harvest_impact(target_pos, actor_color, Fade(color_darken(actor_color, 0.3F), 0.0F),
                                                     HARVEST_IMPACT_SIZE_MULTIPLIER, HARVEST_IMPACT_PARTICLE_COUNT);

                // Reset particle spawn timer for active effect
                behavior->particle_spawn_timer = 0.0F;
            }
        } break;

        case ENTITY_BEHAVIOR_STATE_DELIVERING_TO_LUMBERYARD: {
            behavior->action_timer = ACTION_DURATION_DELIVERY;

            i_movement_set_goal_stop(id);

            if (i_is_target_valid(behavior->target_id, behavior->target_gen)) {
                i_movement_set_goal_turn_to_target(id, g_world->position[behavior->target_id], true, EASE_IN_OUT_CUBIC);
            }
        } break;

        case ENTITY_BEHAVIOR_STATE_ATTACKING_NPC: {
            behavior->action_timer = asset_get_animation_duration_by_hash(g_world->model_name_hash[id], 3, g_world->animation[id].anim_fps, CESIUM_ANIMATION_SPEED);

            i_movement_set_goal_stop(id);

            if (i_is_target_valid(behavior->target_id, behavior->target_gen)) {
                i_movement_set_goal_turn_to_target(id, g_world->position[behavior->target_id], true, EASE_IN_OUT_CUBIC);
            }
        } break;

        default: {
            break;
        }
    }

    if (c_world__verbose_actors) {
        lln("\\ouc{#00ffaaff}[%04d] "
            "\\ouc{#ff6b6bff}%-28s "
            "\\ouc{#ffff00ff}-> "
            "\\ouc{#8e0ff4ff}%-32s "
            "\\ouc{#888888ff}(%s)",
            id, i_behavior_state_strings[old_state], i_behavior_state_strings[new_state], reason);
    }

    behavior->state = new_state;

    world_mark_follower_cache_dirty();
}

void entity_actor_search_and_transition_to_target(EID id, C8 const *reason) {
    EntityBehaviorController *behavior = &g_world->actor[id].behavior;

    // Check if we should switch to lumberyard
    if (behavior->target_entity_type == ENTITY_TYPE_VEGETATION && i_actor_is_full(id)) { behavior->target_entity_type = ENTITY_TYPE_BUILDING_LUMBERYARD; }

    EID const target_id = i_find_target(id, behavior->target_entity_type);

    if (target_id != INVALID_EID) {
        Vector3 const actor_pos  = g_world->position[id];
        Vector3 const target_pos = g_world->position[target_id];

        behavior->target_position = target_pos;
        behavior->target_id       = target_id;
        behavior->target_gen      = g_world->generation[target_id];
        world_target_tracker_add(target_id, id);

        C8 const *type_name = entity_type_to_cstr(behavior->target_entity_type);
        entity_actor_behavior_transition_to_state(id, ENTITY_BEHAVIOR_STATE_GOING_TO_TARGET, TS("%s - Found %s %04d", reason, type_name, target_id)->c);
    } else {
        // No target found - smart fallback
        if (behavior->target_entity_type == ENTITY_TYPE_VEGETATION && behavior->wood_count > 0) {
            behavior->target_entity_type = ENTITY_TYPE_BUILDING_LUMBERYARD;
            entity_actor_search_and_transition_to_target(id, TS("No trees found, delivering wood (%zu)", behavior->wood_count)->c);
        } else {
            C8 const *type_name = entity_type_to_cstr(behavior->target_entity_type);
            entity_actor_behavior_transition_to_state(id, ENTITY_BEHAVIOR_STATE_IDLE, TS("%s - No %ss found, going idle", reason, type_name)->c);
        }
    }
}

void entity_actor_start_looking_for_target(EID id, EntityType target_type) {
    if (!ENTITY_HAS_FLAG(g_world->flags[id], ENTITY_FLAG_ACTOR)) { return; }

    EntityBehaviorController *behavior = &g_world->actor[id].behavior;
    behavior->target_entity_type = target_type;

    if (target_type == ENTITY_TYPE_VEGETATION && i_actor_is_full(id)) { behavior->target_entity_type = ENTITY_TYPE_BUILDING_LUMBERYARD; }

    i_play_agree(id);

    C8 const *type_name = entity_type_to_cstr(behavior->target_entity_type);
    entity_actor_search_and_transition_to_target(id, TS("Commanded to search for %ss", type_name)->c);
}

void entity_actor_set_move_target(EID id, Vector3 target) {
    if (!ENTITY_HAS_FLAG(g_world->flags[id], ENTITY_FLAG_ACTOR)) { return; }

    EntityBehaviorController *behavior = &g_world->actor[id].behavior;
    behavior->target_position = target;
    entity_actor_clear_actor_target(id);
    behavior->target_entity_type = ENTITY_TYPE_NONE;

    i_play_agree(id);

    entity_actor_behavior_transition_to_state(id, ENTITY_BEHAVIOR_STATE_GOING_TO_TARGET, TS("Given target pos (%.1f, %.1f)", target.x, target.z)->c);
}

void entity_actor_set_attack_target_npc(EID attacker_id, EID target_id) {
    if (!ENTITY_HAS_FLAG(g_world->flags[attacker_id], ENTITY_FLAG_ACTOR)) { return; }
    if (!entity_is_valid(target_id))                                      { return; }
    if (g_world->type[target_id] != ENTITY_TYPE_NPC)                      { return; }
    if (attacker_id == target_id)                                         { return; }

    EntityBehaviorController *behavior = &g_world->actor[attacker_id].behavior;
    behavior->target_id          = target_id;
    behavior->target_gen         = g_world->generation[target_id];
    behavior->target_entity_type = ENTITY_TYPE_NPC;
    world_target_tracker_add(target_id, attacker_id);

    i_play_agree(attacker_id);

    Vector3 const attacker_pos = g_world->position[attacker_id];
    Vector3 const target_pos   = g_world->position[target_id];
    F32 const distance         = Vector3Distance(attacker_pos, target_pos);

    if (distance <= ATTACK_RANGE) {
        entity_actor_behavior_transition_to_state(attacker_id, ENTITY_BEHAVIOR_STATE_ATTACKING_NPC, TS("Attacking NPC %04d (%.1fm away)", target_id, distance)->c);
    } else {
        entity_actor_behavior_transition_to_state(attacker_id, ENTITY_BEHAVIOR_STATE_GOING_TO_TARGET, TS("Chasing NPC %04d to attack (%.1fm away)", target_id, distance)->c);
    }
}

void entity_actor_clear_target_tracker(EID target_id) {
    if (target_id >= WORLD_MAX_ENTITIES) { return; }
    g_world->target_trackers[target_id].count = 0;
}

void entity_actor_clear_actor_target(EID actor_id) {
    EntityBehaviorController *behavior = &g_world->actor[actor_id].behavior;
    if (behavior->target_id != INVALID_EID) {
        world_target_tracker_remove(behavior->target_id, actor_id);
        behavior->target_id  = INVALID_EID;
        behavior->target_gen = 0;
    }
}

C8 const *entity_actor_behavior_state_to_cstr(EntityBehaviorState state) {
    return i_behavior_state_strings[state];
}

C8 const *entity_actor_movement_state_to_cstr(EntityMovementState state) {
    return i_movement_state_strings[state];
}

C8 const *entity_actor_movement_goal_to_cstr(EntityMovementGoal goal) {
    return i_movement_goal_strings[goal];
}

void entity_actor_draw_2d_hud(EID id) {
    if (!c_world__actor_info) { return; }

    BOOL const is_selected = g_world->selected_id == id;
    if (!is_selected) {
        if (!ENTITY_HAS_FLAG(g_world->flags[id], ENTITY_FLAG_IN_FRUSTUM)) { return; }
        if (g_world->type[id] != ENTITY_TYPE_NPC) { return; }
    }

    EntityBehaviorController const *behavior = &g_world->actor[id].behavior;

    Camera3D *cam            = c3d_get_ptr();
    Vector2 const render_res = render_get_render_resolution();

    Vector3 const position        = g_world->position[id];
    OrientedBoundingBox const obb = g_world->obb[id];

    Vector3 const world_pos  = {position.x, position.y + (obb.extents.y * 2.25F), position.z};
    Vector2 const screen_pos = GetWorldToScreenEx(world_pos, *cam, (S32)render_res.x, (S32)render_res.y);

    if (!is_selected) {
        if (screen_pos.x < -100 || screen_pos.x > render_res.x + 100 || screen_pos.y < -100 || screen_pos.y > render_res.y + 100) { return; }
    }

    F32 const distance_to_player  = Vector3Distance(position, cam->position);
    F32 const max_hud_distance    = 30.0F;
    F32 const fade_start_distance = 25.0F;

    if (!is_selected) {
        if (distance_to_player > max_hud_distance) { return; }
    }

    F32 distance_progress = 0.0F;
    if (distance_to_player > fade_start_distance) { distance_progress = (distance_to_player - fade_start_distance) / (max_hud_distance - fade_start_distance); }

    F32 alpha = ease_out_back(1.0F - distance_progress, 0.0F, 1.0F, 1.0F);
    if (is_selected) { alpha = 1.0F; }

    F32 const darken_amount = is_selected ? 0.6F : 0.9F;

    Color const tint          = g_world->tint[id];
    Color const bg_color      = Fade(color_darken(tint, darken_amount), 0.95F * alpha);
    Color const bg_line_color = Fade(tint, 0.95F * alpha);

    F32 const scale_factor = 0.8F + (0.2F * alpha);

    Color state_color;
    switch (behavior->state) {
        case ENTITY_BEHAVIOR_STATE_IDLE:                     state_color = Fade({130, 170, 220, 255}, alpha); break;
        case ENTITY_BEHAVIOR_STATE_GOING_TO_TARGET:          state_color = Fade({245, 190, 85, 255}, alpha);  break;
        case ENTITY_BEHAVIOR_STATE_HARVESTING_TARGET:        state_color = Fade({100, 190, 100, 255}, alpha); break;
        case ENTITY_BEHAVIOR_STATE_DELIVERING_TO_LUMBERYARD: state_color = Fade({80, 210, 120, 255}, alpha);  break;
        case ENTITY_BEHAVIOR_STATE_ATTACKING_NPC:            state_color = Fade({220, 50, 50, 255}, alpha);   break;
        default:                                             state_color = Fade({255, 255, 255, 255}, alpha); break;
    }

    C8 const *name_text       = g_world->name[id];
    String const *state_text  = nullptr;
    String const *detail_text = nullptr;
    F32 progress              = -1.0F;
    Color progress_color      = {120, 180, 120, 255};

    switch (behavior->state) {
        case ENTITY_BEHAVIOR_STATE_IDLE: {
            state_text     = TS("RESTING");
            detail_text    = TS("Standing by");
            progress_color = {100, 150, 200, 255};
        } break;

        case ENTITY_BEHAVIOR_STATE_GOING_TO_TARGET: {
            state_text = TS("TRAVELING");
            if (entity_is_valid(behavior->target_id)) {
                F32 const distance = Vector3Distance(position, behavior->target_position);
                detail_text = TS("Target nearby (%.1fm)", distance);
            } else {
                F32 const distance = Vector3Distance(position, behavior->target_position);
                detail_text = TS("Moving (%.1fm to go)", distance);
            }
            progress_color = {200, 150, 80, 255};
        } break;

        case ENTITY_BEHAVIOR_STATE_HARVESTING_TARGET: {
            state_text     = TS("HARVESTING");
            progress       = glm::clamp(1.0F - (behavior->action_timer / ACTION_DURATION_HARVEST), 0.0F, 1.0F);
            progress_color = {120, 180, 120, 255};
            detail_text    = TS("Gathering resources (%.1fs)", behavior->action_timer);
        } break;

        case ENTITY_BEHAVIOR_STATE_DELIVERING_TO_LUMBERYARD: {
            state_text     = TS("DELIVERING");
            progress       = glm::clamp(1.0F - (behavior->action_timer / ACTION_DURATION_DELIVERY), 0.0F, 1.0F);
            progress_color = {90, 238, 90, 255};
            detail_text    = TS("Dropping off wood (%.1fs)", behavior->action_timer);
        } break;

        case ENTITY_BEHAVIOR_STATE_ATTACKING_NPC: {
            state_text     = TS("ATTACKING");
            F32 const attack_duration = asset_get_animation_duration_by_hash(g_world->model_name_hash[id], 3, g_world->animation[id].anim_fps, CESIUM_ANIMATION_SPEED);
            progress       = glm::clamp(1.0F - (behavior->action_timer / attack_duration), 0.0F, 1.0F);
            progress_color = {220, 50, 50, 255};
            detail_text    = TS("Combat with %s (%.1fs)", g_world->name[behavior->target_id], behavior->action_timer);
        } break;

        default: {
            state_text  = TS("UNKNOWN");
            detail_text = TS("Invalid state");
        } break;
    }

    Color const name_color   = Fade({255, 215, 225, 255}, alpha);
    Color const detail_color = Fade({25, 215, 225, 255}, alpha);
    Color const reason_color = Fade({165, 170, 185, 255}, alpha);

    F32 const name_font_size   = PERC(1.20F, render_res.x) * scale_factor;
    F32 const state_font_size  = PERC(1.10F, render_res.x) * scale_factor;
    F32 const reason_font_size = PERC(0.90F, render_res.x) * scale_factor;
    F32 const detail_font_size = PERC(1.00F, render_res.x) * scale_factor;

    AFont *name_font   = asset_get_font("GoMono", (S32)name_font_size);
    AFont *state_font  = asset_get_font("GoMono", (S32)state_font_size);
    AFont *reason_font = asset_get_font("GoMono", (S32)reason_font_size);
    AFont *detail_font = asset_get_font("GoMono", (S32)detail_font_size);

    Vector2 const name_text_size   = measure_text(name_font, name_text);
    Vector2 const state_text_size  = measure_text(state_font, state_text->c);
    Vector2 const detail_text_size = measure_text_ouc(detail_font, detail_text->c);
    Vector2 const reason_text_size = measure_text_ouc(reason_font, behavior->state_reason);

    F32 const name_width   = name_text_size.x;
    F32 const state_width  = state_text_size.x;
    F32 const reason_width = reason_text_size.x;
    F32 const detail_width = detail_text_size.x;

    F32 const name_height   = name_text_size.y;
    F32 const state_height  = state_text_size.y;
    F32 const reason_height = reason_text_size.y;
    F32 const detail_height = detail_text_size.y;

    F32 const padding             = PERC(0.4F, render_res.x) * scale_factor;
    F32 const text_spacing        = PERC(0.3F, render_res.x) * scale_factor;
    F32 const progress_spacing    = PERC(0.4F, render_res.x) * scale_factor;
    F32 const progress_bar_height = PERC(0.5F, render_res.x) * scale_factor;
    F32 const progress_bar_margin = PERC(0.5F, render_res.x) * scale_factor;

    F32 const min_panel_width    = PERC(5.0F, render_res.x) * scale_factor;
    F32 const max_content_width  = glm::max(name_width, glm::max(glm::max(state_width, detail_width), reason_width));
    F32 const panel_width        = glm::max(min_panel_width, max_content_width + (padding * 2.0F));
    F32 const progress_bar_width = panel_width - (progress_bar_margin * 2.0F);

    F32 panel_height = (padding * 2.0F) + name_height + text_spacing + state_height + text_spacing + detail_height + text_spacing + reason_height;
    if (progress >= 0.0F) { panel_height += progress_spacing + progress_bar_height; }

    Vector2 const panel_pos = {screen_pos.x - (panel_width * 0.5F), screen_pos.y - panel_height - PERC(0.6F, render_res.x)};
    Rectangle const bg_rec  = {panel_pos.x, panel_pos.y, panel_width, panel_height};

    d2d_rectangle_rounded_rec(bg_rec, 0.3F, 8, bg_color);
    d2d_rectangle_rounded_rec_lines(bg_rec, 0.3F, 8, bg_line_color);

    F32 current_y = panel_pos.y + padding;

    Vector2 const name_text_pos = {panel_pos.x + ((panel_width - name_width) * 0.5F), current_y};
    d2d_text_ouc(name_font, name_text, name_text_pos, name_color);
    current_y += name_height + text_spacing;

    Vector2 const state_text_pos = {panel_pos.x + ((panel_width - state_width) * 0.5F), current_y};
    d2d_text_ouc(state_font, state_text->c, state_text_pos, state_color);
    current_y += state_height + text_spacing;

    Vector2 const reason_text_pos = {panel_pos.x + ((panel_width - reason_width) * 0.5F), current_y};
    d2d_text_ouc(reason_font, behavior->state_reason, reason_text_pos, reason_color);
    current_y += reason_height + text_spacing;

    Vector2 const detail_text_pos = {panel_pos.x + ((panel_width - detail_width) * 0.5F), current_y};
    d2d_text_ouc(detail_font, detail_text->c, detail_text_pos, detail_color);
    current_y += detail_height;

    if (progress >= 0.0F) {
        current_y += progress_spacing;

        Vector2 const progress_pos  = {panel_pos.x + progress_bar_margin, current_y};
        Rectangle const progress_bg = {progress_pos.x, progress_pos.y, progress_bar_width, progress_bar_height};

        d2d_rectangle_rounded_rec(progress_bg, 0.5F, 4, Fade({30, 35, 42, 200}, alpha));

        F32 const filled_width = progress_bar_width * progress;
        if (filled_width > 2.0F) {
            Rectangle const progress_fill = {progress_pos.x, progress_pos.y, filled_width, progress_bar_height};
            d2d_rectangle_rounded_rec(progress_fill, 0.5F, 4, Fade(progress_color, alpha));

            Rectangle const progress_highlight = {progress_fill.x, progress_fill.y, progress_fill.width, progress_fill.height * 0.3F};
            Color highlight_color = progress_color;
            highlight_color.r = (U8)glm::min(255, progress_color.r + 30);
            highlight_color.g = (U8)glm::min(255, progress_color.g + 30);
            highlight_color.b = (U8)glm::min(255, progress_color.b + 30);
            d2d_rectangle_rounded_rec(progress_highlight, 0.5F, 4, Fade(highlight_color, alpha * 0.5F));
        }

        d2d_rectangle_rounded_rec_lines(progress_bg, 0.5F, 4, Fade(color_darken(progress_color, 0.33F), alpha));
    }
}
