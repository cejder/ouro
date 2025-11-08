#include "world.hpp"
#include "asset.hpp"
#include "audio.hpp"
#include "color.hpp"
#include "cvar.hpp"
#include "edit.hpp"
#include "log.hpp"
#include "map.hpp"
#include "message.hpp"
#include "particles_3d.hpp"
#include "render.hpp"
#include "std.hpp"
#include "string.hpp"
#include "time.hpp"

#include <errno.h>
#include <raymath.h>
#include <string.h>
#include <sys/stat.h>

WorldState g_world_state = {};
World* g_world = g_world_state.current;

// Instanced rendering: map from model name to array of entity IDs
MAP_DECLARE(InstanceGroupMap, C8 const *, EIDArray, MAP_HASH_CSTR, MAP_EQUAL_CSTR);

// Minimum number of instances to use instanced rendering (avoids overhead for single entities)
#define MIN_INSTANCE_COUNT 2

// Helper: Find bone index by name (case-insensitive search)
// Returns -1 if bone not found
S32 static i_find_bone_index(AModel *model, C8 const *bone_name) {
    if (!model || !model->base.bones) { return -1; }

    for (S32 i = 0; i < model->base.boneCount; i++) {
        if (ou_strcmp(model->base.bones[i].name, bone_name) == 0) {
            return i;
        }
    }
    return -1;
}

// Helper: Get world-space position of a bone from current animation state
// Returns true if bone was found and position was calculated
BOOL static i_get_bone_world_position(EID id, C8 const *bone_name, Vector3 *out_position) {
    AModel *model = asset_get_model(g_world->model_name[id]);
    if (!model || !model->has_animations) { return false; }

    S32 const bone_idx = i_find_bone_index(model, bone_name);
    if (bone_idx < 0 || bone_idx >= g_world->animation[id].bone_count) { return false; }

    // Get the current bone matrix (already in model space from animation)
    Matrix const bone_matrix = g_world->animation[id].bone_matrices[bone_idx];

    // Extract translation from the bone matrix
    Vector3 bone_local_pos = {bone_matrix.m12, bone_matrix.m13, bone_matrix.m14};

    // Transform to world space using entity transform
    F32 const rotation = g_world->rotation[id];
    Vector3 const scale = g_world->scale[id];
    F32 const rotation_rad = rotation * DEG2RAD;

    // Apply entity rotation
    F32 const cos_r = math_cos_f32(rotation_rad);
    F32 const sin_r = math_sin_f32(rotation_rad);
    Vector3 rotated = {
        (bone_local_pos.x * cos_r) - (bone_local_pos.z * sin_r),
        bone_local_pos.y,
        (bone_local_pos.x * sin_r) + (bone_local_pos.z * cos_r)
    };

    // Apply entity scale and position
    out_position->x = g_world->position[id].x + (rotated.x * scale.x);
    out_position->y = g_world->position[id].y + (rotated.y * scale.y);
    out_position->z = g_world->position[id].z + (rotated.z * scale.z);

    return true;
}

BOOL static i_get_bone_by_name(EID id, C8 const *bone_name, Vector3 *out_position) {
    AModel *model = asset_get_model(g_world->model_name[id]);
    if (!model || !model->has_animations) { return false; }

    S32 const bone_count = g_world->animation[id].bone_count;
    if (bone_count <= 0) { return false; }

    for (S32 bone_idx = 0; bone_idx < bone_count; bone_idx++) {
        if (ou_strcmp(model->base.bones[bone_idx].name, bone_name)) { continue; }

        // Get the current bone matrix
        Matrix const bone_matrix = g_world->animation[id].bone_matrices[bone_idx];
        // Extract translation from the bone matrix
        Vector3 bone_local_pos = {bone_matrix.m12, bone_matrix.m13, bone_matrix.m14};
        // Transform to world space
        F32 const rotation     = g_world->rotation[id];
        Vector3 const scale    = g_world->scale[id];
        F32 const rotation_rad = rotation * DEG2RAD;

        F32 const cos_r = math_cos_f32(rotation_rad);
        F32 const sin_r = math_sin_f32(rotation_rad);
        Vector3 rotated = {
            (bone_local_pos.x * cos_r) - (bone_local_pos.z * sin_r),
            bone_local_pos.y,
            (bone_local_pos.x * sin_r) + (bone_local_pos.z * cos_r)
        };

        Vector3 world_pos;
        world_pos.x = g_world->position[id].x + (rotated.x * scale.x);
        world_pos.y = g_world->position[id].y + (rotated.y * scale.y);
        world_pos.z = g_world->position[id].z + (rotated.z * scale.z);

        *out_position = world_pos;
        return true;
    }

    return false;
}

// Helper: Decompose a 4x4 matrix into translation, rotation (quaternion), and scale
// Note: Assumes matrix is TRS (Translation * Rotation * Scale) with no skew
void static i_matrix_decompose(Matrix mat, Vector3 *translation, Quaternion *rotation, Vector3 *scale) {
    // Extract translation (last column)
    translation->x = mat.m12;
    translation->y = mat.m13;
    translation->z = mat.m14;

    // Extract scale (magnitude of basis vectors)
    Vector3 scale_x = {mat.m0, mat.m1, mat.m2};
    Vector3 scale_y = {mat.m4, mat.m5, mat.m6};
    Vector3 scale_z = {mat.m8, mat.m9, mat.m10};

    scale->x = Vector3Length(scale_x);
    scale->y = Vector3Length(scale_y);
    scale->z = Vector3Length(scale_z);

    // Extract rotation matrix (remove scale from basis vectors)
    Matrix rot_mat = mat;
    if (scale->x != 0.0F) {
        rot_mat.m0 /= scale->x;
        rot_mat.m1 /= scale->x;
        rot_mat.m2 /= scale->x;
    }
    if (scale->y != 0.0F) {
        rot_mat.m4 /= scale->y;
        rot_mat.m5 /= scale->y;
        rot_mat.m6 /= scale->y;
    }
    if (scale->z != 0.0F) {
        rot_mat.m8 /= scale->z;
        rot_mat.m9 /= scale->z;
        rot_mat.m10 /= scale->z;
    }

    // Convert rotation matrix to quaternion
    *rotation = QuaternionFromMatrix(rot_mat);
}

// Compute bone matrices for entity based on its animation state
// Similar to UpdateModelAnimationBones but writes to per-entity storage
// Supports blending between animations for smooth transitions
void static i_compute_entity_bone_matrices(EID id) {
    AModel *model = asset_get_model(g_world->model_name[id]);

    // Skip if model has no animations or bones
    if (!model || !model->has_animations) { return; }

    U32 const anim_idx = g_world->animation[id].anim_index;
    U32 const frame    = g_world->animation[id].anim_frame;

    // Validate animation index
    if (anim_idx >= (U32)model->animation_count) { return; }

    ModelAnimation const& anim = model->animations[anim_idx];

    // Validate frame
    if (frame >= (U32)anim.frameCount) { return; }

    // Compute bone matrices for current (new) animation
    Matrix new_bone_matrices[ENTITY_MAX_BONES];
    for (S32 bone_id = 0; bone_id < anim.boneCount && bone_id < ENTITY_MAX_BONES; bone_id++) {
        Transform *bind_transform = &model->base.bindPose[bone_id];
        Matrix bind_matrix        = MatrixMultiply(MatrixMultiply(
            MatrixScale(bind_transform->scale.x, bind_transform->scale.y, bind_transform->scale.z),
            QuaternionToMatrix(bind_transform->rotation)),
            MatrixTranslate(bind_transform->translation.x, bind_transform->translation.y, bind_transform->translation.z));

        Transform *target_transform = &anim.framePoses[frame][bone_id];
        Matrix target_matrix        = MatrixMultiply(MatrixMultiply(
            MatrixScale(target_transform->scale.x, target_transform->scale.y, target_transform->scale.z),
            QuaternionToMatrix(target_transform->rotation)),
            MatrixTranslate(target_transform->translation.x, target_transform->translation.y, target_transform->translation.z));

        new_bone_matrices[bone_id] = MatrixMultiply(MatrixInvert(bind_matrix), target_matrix);
    }

    // If blending, interpolate between previous and new bone matrices
    if (g_world->animation[id].is_blending) {
        F32 blend_t = g_world->animation[id].blend_time / g_world->animation[id].blend_duration;
        blend_t = glm::clamp(blend_t, 0.0F, 1.0F);

        S32 bone_count = anim.boneCount < ENTITY_MAX_BONES ? anim.boneCount : ENTITY_MAX_BONES;
        for (S32 bone_id = 0; bone_id < bone_count; bone_id++) {
            // Decompose both matrices into TRS components
            Vector3 prev_trans;
            Vector3 new_trans;
            Vector3 prev_scale;
            Vector3 new_scale;
            Quaternion prev_rot;
            Quaternion new_rot;

            i_matrix_decompose(g_world->animation[id].prev_bone_matrices[bone_id], &prev_trans, &prev_rot, &prev_scale);
            i_matrix_decompose(new_bone_matrices[bone_id], &new_trans, &new_rot, &new_scale);

            // Interpolate components
            Vector3 blended_trans = Vector3Lerp(prev_trans, new_trans, blend_t);
            Quaternion blended_rot = QuaternionSlerp(prev_rot, new_rot, blend_t);
            Vector3 blended_scale = Vector3Lerp(prev_scale, new_scale, blend_t);

            // Reconstruct blended matrix
            g_world->animation[id].bone_matrices[bone_id] = MatrixMultiply(MatrixMultiply(
                MatrixScale(blended_scale.x, blended_scale.y, blended_scale.z),
                QuaternionToMatrix(blended_rot)),
                MatrixTranslate(blended_trans.x, blended_trans.y, blended_trans.z));
        }
    } else {
        // No blending, just copy new matrices
        S32 bone_count = anim.boneCount < ENTITY_MAX_BONES ? anim.boneCount : ENTITY_MAX_BONES;
        ou_memcpy(g_world->animation[id].bone_matrices, new_bone_matrices, sizeof(Matrix) * (SZ)bone_count);
    }
}

void world_init() {
    // Reset both worlds and default on start on overworld
    g_world = &g_world_state.dungeon;
    world_reset();

    g_world = &g_world_state.overworld;
    world_reset();

    world_recorder_init();
    entity_init();
    grid_init({A_TERRAIN_DEFAULT_SIZE, A_TERRAIN_DEFAULT_SIZE}); // TODO: We need to make sure that we set the proper size. Right now we are defaulting.
    g_world_state.initialized = true;
}

// TODO: This basically is just a retarded dumb idea. We are technically not reseting anything reliably idk man. We should set flags for sub things like talker
// and when we set the flag to enable or something we also reset idk.
void world_reset() {
    for (EID i = 0; i < WORLD_MAX_ENTITIES; ++i) {
        g_world->flags[i]          = 0;
        g_world->type[i]           = ENTITY_TYPE_NONE;
        g_world->lifetime[i]       = 0.0F;
        g_world->position[i]       = {};
        g_world->scale[i]          = {};
        g_world->original_scale[i] = {};
        g_world->rotation[i]       = {};
        g_world->obb[i]            = {};
        g_world->radius[i]         = 0.0F;
        g_world->model_name[i][0]  = '\0';
        g_world->tint[i]           = {};
        g_world->generation[i]     = 0;
        g_world->talker[i]         = {};
        g_world->building[i]       = {};
        g_world->name[i][0]        = '\0';
    }

    g_world->active_ent_count    = 0;
    g_world->active_entity_count = 0;
    g_world->max_gen             = 0;
    g_world->selected_id         = INVALID_EID;

    grid_clear();
}

void world_update(F32 dt, F32 dtu) {
    g_world->visible_vertex_count = 0;

    world_recorder_update();  // WARN: This needs to happen before anything that changes the world.
    grid_populate();
    edit_update(dt, dtu);

    for (U32 &count : g_world->entity_type_counts) { count = 0; }

    // Use active entities from last frame
    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (!ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_IN_USE)) { continue; }  // Safety check for entities that died during edit update

        g_world->lifetime[i] += dt;
        g_world->entity_type_counts[g_world->type[i]]++;

        c3d_is_obb_in_frustum(g_world->obb[i]) ? ENTITY_SET_FLAG(g_world->flags[i], ENTITY_FLAG_IN_FRUSTUM)
                                              : ENTITY_CLEAR_FLAG(g_world->flags[i], ENTITY_FLAG_IN_FRUSTUM);

        if (ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_IN_FRUSTUM)) { g_world->visible_vertex_count += asset_get_model(g_world->model_name[i])->vertex_count; }
        if (ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_ACTOR))      { entity_actor_update(i, dt); }
        if (g_world->type[i] == ENTITY_TYPE_BUILDING_LUMBERYARD)        { entity_building_update(i, dt); }

#if OURO_TALK
        if (g_world->type[i] == ENTITY_TYPE_NPC) {
            F32 const width = g_world->obb[i].extents.x * 2.0F;
            talker_update(&g_world->talker[i], dt, g_world->position[i], width);
        }
#endif
    }

    // Build active entities array for draw functions to use
    g_world->active_entity_count = 0;
    for (EID i = 0; i < WORLD_MAX_ENTITIES; ++i) {
        if (ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_IN_USE)) { g_world->active_entities[g_world->active_entity_count++] = i; }
    }

    // Update all entity animations
    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const id = g_world->active_entities[idx];

        if (!g_world->animation[id].has_animations) { continue; }
        if (!g_world->animation[id].anim_playing)   { continue; }

        AModel *model      = asset_get_model(g_world->model_name[id]);
        U32 const anim_idx = g_world->animation[id].anim_index;

        if (anim_idx >= (U32)model->animation_count) { continue; }

        ModelAnimation const& anim = model->animations[anim_idx];

        // Update animation time (frame-time independent)
        g_world->animation[id].anim_time += dt * g_world->animation[id].anim_speed;

        // Calculate frame from time using entity's animation FPS
        F32 const fps            = g_world->animation[id].anim_fps;
        F32 const frame_duration = 1.0F / fps;
        F32 const total_duration = (F32)anim.frameCount * frame_duration;

        // Handle looping
        if (g_world->animation[id].anim_time >= total_duration) {
            if (g_world->animation[id].anim_loop) {
                // Preserve overflow time for smooth looping
                g_world->animation[id].anim_time = math_mod_f32(g_world->animation[id].anim_time, total_duration);
            } else {
                g_world->animation[id].anim_time = total_duration - frame_duration;
                g_world->animation[id].anim_playing = false;
            }
        }

        // Convert time to frame (always valid due to loop handling above)
        U32 new_frame                     = (U32)(g_world->animation[id].anim_time * fps);
        new_frame                         = glm::min(new_frame, (U32)anim.frameCount - 1);
        g_world->animation[id].anim_frame = new_frame;

        // Update blend timer if blending
        if (g_world->animation[id].is_blending) {
            g_world->animation[id].blend_time += dt;
            if (g_world->animation[id].blend_time >= g_world->animation[id].blend_duration) {
                g_world->animation[id].is_blending = false;  // Blending complete
            }
        }

        // Compute bone matrices for this entity
        i_compute_entity_bone_matrices(id);
    }
}

void world_draw_2d() {
    // for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
    //     EID const i = g_world->active_entities[idx];
    //     unused(i);
    // }
}

void world_draw_2d_hud() {
    world_recorder_draw_2d_hud();

    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];

        // NOTE: Talker stuff is independent of frustum culling.
        talker_draw(&g_world->talker[i], g_world->name[i]);

        if (!ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_IN_FRUSTUM)) { continue; }

        if (g_world->type[i] == ENTITY_TYPE_NPC) {
            entity_actor_draw_2d_hud(i);
            d2d_healthbar(i);
        }
    }
}

void world_draw_2d_dbg() {
    if (!c_debug__enabled) { return; }

    grid_draw_2d_dbg();

    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (!ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_IN_FRUSTUM)) { continue; }

        BOOL const is_selected = (g_world->selected_id == i);

        if (c_debug__gizmo_info && is_selected) { d2d_gizmo(i); }

        if (c_debug__bone_info && is_selected && g_world->animation[i].has_animations) {
            d2d_bone_gizmo(i);
        }
    }
}

void world_draw_3d() {
    // for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
    //     EID const i = g_world->active_entities[idx];
    //     unused(i);
    // }
}

// Backpack positioning constants
// How far behind the actor (negative forward direction)
#define BACKPACK_OFFSET_BACKWARD 0.65F
// How far above the actor's base position
#define BACKPACK_OFFSET_UPWARD 1.5F
// Scale of the backpack relative to actor scale
#define BACKPACK_MAX_SCALE  0.2F

void world_draw_3d_sketch() {
    F32 const bp_base_scale = BACKPACK_MAX_SCALE*0.5F;

    // Group static entities by model name for instanced rendering
    InstanceGroupMap instance_groups;
    InstanceGroupMap_init(&instance_groups, MEMORY_TYPE_ARENA_TRANSIENT, 32);

    // First pass: Group static entities, draw animated entities immediately
    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (!ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_IN_FRUSTUM)) { continue; }

        // Check if entity has active animation
        if (g_world->animation[i].has_animations) {
            // Animated entities: draw immediately (no instancing for now)
            d3d_model_animated(
                g_world->model_name[i],
                g_world->position[i],
                g_world->rotation[i],
                g_world->scale[i],
                g_world->tint[i],
                g_world->animation[i].bone_matrices,
                g_world->animation[i].bone_count
            );
        } else {
            // Static entities: group by model name
            C8 const *model_name = g_world->model_name[i];
            EIDArray *group = InstanceGroupMap_get(&instance_groups, model_name);
            if (!group) {
                EIDArray new_group;
                array_init(MEMORY_TYPE_ARENA_TRANSIENT, &new_group, 64);
                InstanceGroupMap_insert(&instance_groups, model_name, new_group);
                group = InstanceGroupMap_get(&instance_groups, model_name);
            }
            array_push(group, i);
        }

        // Draw carried resources on actor's back
        if (ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_ACTOR)) {
            SZ const wood_count = g_world->actor[i].behavior.wood_count;
            if (wood_count > 0) {
                F32 const rotation = g_world->rotation[i];
                Vector3 scale = g_world->scale[i];
                Vector3 backpack_pos;

                // TODO: Actually what we want:
                // C8 const *bone_name = "socket_hat";
                // if (!i_get_bone_by_name(i, bone_name, &backpack_pos)) {
                //     llw("bone '%s' not found!", bone_name);
                // }

                // What works for now:
                F32 const rotation_rad  = rotation * DEG2RAD;
                Vector3 forward         = {math_sin_f32(rotation_rad), 0.0F, math_cos_f32(rotation_rad)};
                Vector3 up              = {0.0F, 1.0F, 0.0F};
                Vector3 backpack_offset = Vector3Scale(forward, -BACKPACK_OFFSET_BACKWARD * scale.z);  // Behind
                backpack_offset         = Vector3Add(backpack_offset, Vector3Scale(up, BACKPACK_OFFSET_UPWARD * scale.y));  // Up
                backpack_pos            = Vector3Add(g_world->position[i], backpack_offset);

                // Calculate backpack scale with delivery animation
                F32 backpack_scale_multiplier = bp_base_scale + (bp_base_scale * ((F32)wood_count / (F32)ACTOR_WOOD_COLLECTED_MAX));

                // If delivering, scale down the backpack with easing
                EntityBehaviorController const &behavior = g_world->actor[i].behavior;
                if (behavior.state == ENTITY_BEHAVIOR_STATE_DELIVERING_TO_LUMBERYARD) {
                    // Get proper timing values
                    F32 const time_elapsed = ACTION_DURATION_DELIVERY - behavior.action_timer;
                    F32 const duration     = ACTION_DURATION_DELIVERY;

                    // Easing parameters: ease(t, b, c, d)
                    // t = current time, b = start value, c = change in value, d = duration
                    // Use ease_in_expo for end-heavy animation (keeps scale high, then drops quickly at the end)
                    F32 const scale_factor     = ease_in_expo(time_elapsed, 1.0F, -1.0F, duration);
                    backpack_scale_multiplier *= glm::clamp(scale_factor, 0.0F, 1.0F);
                }

                // TODO: Right now we usually scaled by entity scale but we wont for now.
                scale.x = 1.0F;
                scale.y = 1.0F;
                scale.z = 1.0F;
                Vector3 backpack_scale = Vector3Scale(scale, backpack_scale_multiplier);
                Color wood_color       = {139, 90, 43, 255};

                d3d_model("wood.glb", backpack_pos, rotation, backpack_scale, wood_color);
            }
        }
    }

    // Second pass: Batch render static entity groups
    C8 const *model_name;
    EIDArray group;
    MAP_EACH(&instance_groups, model_name, group) {
        if (group.count < MIN_INSTANCE_COUNT) {
            // Not worth instancing for single/few entities - use regular rendering
            for (SZ j = 0; j < group.count; ++j) {
                EID const i = group.data[j];
                d3d_model(
                    model_name,
                    g_world->position[i],
                    g_world->rotation[i],
                    g_world->scale[i],
                    g_world->tint[i]
                );
            }
        } else {
            // Build transform and color arrays for all instances in this group
            MatrixArray transforms;
            ColorArray tints;
            array_init(MEMORY_TYPE_ARENA_TRANSIENT, &transforms, group.count);
            array_init(MEMORY_TYPE_ARENA_TRANSIENT, &tints, group.count);

            for (SZ j = 0; j < group.count; ++j) {
                EID const i = group.data[j];
                Matrix mat_scale = MatrixScale(g_world->scale[i].x, g_world->scale[i].y, g_world->scale[i].z);
                Matrix mat_rot = MatrixRotate((Vector3){0, 1, 0}, g_world->rotation[i] * DEG2RAD);
                Matrix mat_trans = MatrixTranslate(g_world->position[i].x, g_world->position[i].y, g_world->position[i].z);
                Matrix transform = MatrixMultiply(MatrixMultiply(mat_scale, mat_rot), mat_trans);
                array_push(&transforms, transform);
                array_push(&tints, g_world->tint[i]);
            }

            // Draw all instances with a single draw call!
            d3d_model_instanced(model_name, transforms.data, tints.data, transforms.count);
        }
    }
}

void world_draw_3d_hud() {
    // for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
    //     EID const i = g_world->active_entities[idx];
    //     unused(i);
    // }
}

void world_draw_3d_dbg() {
    if (!c_debug__enabled) { return; }

    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (!ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_IN_FRUSTUM)) { continue; }

        F32 gizmos_alpha       = 1.0F;
        BOOL const is_selected = g_world->selected_id == i;
        Color const obb_color  = is_selected ? color_sync_blinking_regular(SALMON, PERSIMMON) : entity_type_to_color(g_world->type[i]);

        // We only do this fading by distance if the entity is not selected.
        if (!is_selected) {
            // If the target is more than X units, we can fade the gizmos. This is to avoid cluttering the screen.
            F32 const dist_till_no_fade = 75.0F;
            F32 const dist_at_full_fade = 450.0F;
            F32 const dist_to_player    = world_get_distance_to_player(g_world->position[i]);  // Use physics-aware position

            if (dist_to_player > dist_till_no_fade) {
                gizmos_alpha = glm::min(1.0F, glm::max(0.0F, 1.0F - ((dist_to_player - dist_till_no_fade) / (dist_at_full_fade - dist_till_no_fade))));
            }

            // If the alpha is below or equal 0.0F, then we just do not draw the gizmos.
            if (gizmos_alpha <= 0.0F) { continue; }
        }

        // Draw the original OBB gizmo
        if (c_debug__bbox_info) {
            d3d_gizmo(g_world->position[i], g_world->rotation[i], g_world->obb[i], obb_color, gizmos_alpha, g_world->talker[i].is_enabled, is_selected);
        }

        // Draw bone debug info for selected entity
        if (c_debug__bone_info && is_selected && g_world->animation[i].has_animations) {
            d3d_bone_gizmo(i);
        }
    }
}

void world_set_selected_entity(EID id) {
    // INFO: If we are not in debug, we only allow the selection of NPC at this point
    if (!c_debug__enabled) {
        if (g_world->type[id] != ENTITY_TYPE_NPC) { return; }
    }

    g_world->selected_id = id;
    audio_play(ACG_SFX, "selected_0.ogg");
}

void world_vegetation_collision() {
    // INFO: This stupid func (i_check_vegetation_collision at the time) is stupid and all but it is what brought us the first element of what we
    // would call a game. Is it corny that I marked this spot immediately after trying the change to the scene? OOOOOOOOOH BABY!
    // NOTE: This is me after a few months(?). I don't understand the 2nd half of the sentence. I think I was trying to be funny. I don't know.
    // NOTE: Now optimized to use grid-based spatial queries instead of brute-force O(n) loop.

    Vector3 const player_position  = g_world->player.camera3d.position;
    F32 constexpr collision_radius = PLAYER_RADIUS * 12.0F;

    EID static nearby_entities[GRID_NEARBY_ENTITIES_MAX];
    SZ static nearby_count = 0;

    grid_query_entities_in_radius(player_position, collision_radius, nearby_entities, &nearby_count, GRID_NEARBY_ENTITIES_MAX);

    for (SZ i = 0; i < nearby_count; ++i) {
        EID const entity_id = nearby_entities[i];
        if (g_world->type[entity_id] != ENTITY_TYPE_VEGETATION) { continue; }

        BOOL const was_colliding = ENTITY_HAS_FLAG(g_world->flags[entity_id], ENTITY_FLAG_COLLIDING_PLAYER);
        BOOL const is_colliding  = entity_collides_player(entity_id);

        if (is_colliding && !was_colliding) {
            // Collision started - play sound
            S32 const rustle_count     = 2;
            S32 static last_rustle_idx = -1;
            S32 rustle_idx             = random_s32(0, rustle_count - 1);
            if (rustle_idx == last_rustle_idx) { rustle_idx = (rustle_idx + 1) % rustle_count; }
            last_rustle_idx = rustle_idx;

            String const *rustle_str = TS("rustle_%d.ogg", rustle_idx);
            Vector3 pos              = g_world->position[entity_id];
            pos.y                   += g_world->obb[entity_id].extents.y;

            FMOD::Channel *channel = audio_play_3d_at_position(ACG_SFX, rustle_str->c, pos);
            if (channel) {
                F32 const pitch_shift = random_f32(0.8F, 1.2F);
                channel->setPitch(pitch_shift);
            }
            ENTITY_SET_FLAG(g_world->flags[entity_id], ENTITY_FLAG_COLLIDING_PLAYER);
        } else if (!is_colliding && was_colliding) {
            // Collision ended
            ENTITY_CLEAR_FLAG(g_world->flags[entity_id], ENTITY_FLAG_COLLIDING_PLAYER);
        }
    }
}

void world_set_entity_triangle_collision(EID id, AModel *model, Vector3 offset) {
    if (!entity_is_valid(id)) { return; }

    ENTITY_SET_FLAG(g_world->flags[id], ENTITY_FLAG_TRIANGLE_COLLISION);
    g_world->triangle_collider[id].model = model;
    g_world->triangle_collider[id].offset = offset;
}

BOOL world_triangle_mesh_collision() {
    if (c_debug__noclip) { return false; }

    Vector3 const player_position   = g_world->player.camera3d.position;
    Vector3 const previous_position = g_world->player.previous_position;
    F32 constexpr player_radius     = PLAYER_RADIUS;
    F32 constexpr player_height     = PLAYER_HEIGHT_TO_EYES;

    // Calculate movement this frame for swept collision
    Vector3 const movement    = Vector3Subtract(player_position, previous_position);
    F32 const movement_length = Vector3Length(movement);
    BOOL const is_moving      = movement_length > 0.001F;

    // Broad-phase: query nearby entities with generous radius
    F32 constexpr query_radius = 100.0F;
    EID static nearby_entities[GRID_NEARBY_ENTITIES_MAX];
    SZ static nearby_count     = 0;
    grid_query_entities_in_radius(player_position, query_radius, nearby_entities, &nearby_count, GRID_NEARBY_ENTITIES_MAX);

    // Floor detection: find floor closest to current player position
    BOOL found_floor           = false;
    F32 closest_floor_y        = -F32_MAX;
    F32 closest_floor_distance = F32_MAX;

    // Wall collision: accumulate corrections (swept collision prevents tunneling)
    Vector3 wall_correction = {0.0F, 0.0F, 0.0F};

    for (SZ i = 0; i < nearby_count; ++i) {
        EID const entity_id = nearby_entities[i];
        if (!ENTITY_HAS_FLAG(g_world->flags[entity_id], ENTITY_FLAG_TRIANGLE_COLLISION)) { continue; }

        TriangleMeshCollider const *collider = &g_world->triangle_collider[entity_id];
        AModel *model = collider->model;
        if (!model) { continue; }

        Vector3 const entity_position = g_world->position[entity_id];
        Vector3 const entity_scale    = g_world->scale[entity_id];
        Vector3 const model_position  = Vector3Add(entity_position, collider->offset);

        // Cull entire model using transformed AABB
        BoundingBox const model_aabb = math_transform_aabb(model->bb, model_position, entity_scale);
        if (!math_aabb_sphere_intersection(model_aabb.min, model_aabb.max, player_position, player_radius * 2.0F)) {
            continue;
        }

        // Use simplified collision mesh if available, otherwise fall back to visual mesh
        CollisionMesh const *collision = &model->collision;
        if (!collision->generated || collision->triangle_count == 0) { continue; }

        // Process simplified collision triangles
        for (U32 tri_idx = 0; tri_idx < collision->triangle_count; ++tri_idx) {
            U32 const idx0 = collision->indices[(tri_idx * 3) + 0];
            U32 const idx1 = collision->indices[(tri_idx * 3) + 1];
            U32 const idx2 = collision->indices[(tri_idx * 3) + 2];

            // Transform collision vertices to world space
            Vector3 const v0 = {
                (collision->vertices[(idx0 * 3) + 0] * entity_scale.x) + model_position.x,
                (collision->vertices[(idx0 * 3) + 1] * entity_scale.y) + model_position.y,
                (collision->vertices[(idx0 * 3) + 2] * entity_scale.z) + model_position.z
            };
            Vector3 const v1 = {
                (collision->vertices[(idx1 * 3) + 0] * entity_scale.x) + model_position.x,
                (collision->vertices[(idx1 * 3) + 1] * entity_scale.y) + model_position.y,
                (collision->vertices[(idx1 * 3) + 2] * entity_scale.z) + model_position.z
            };
            Vector3 const v2 = {
                (collision->vertices[(idx2 * 3) + 0] * entity_scale.x) + model_position.x,
                (collision->vertices[(idx2 * 3) + 1] * entity_scale.y) + model_position.y,
                (collision->vertices[(idx2 * 3) + 2] * entity_scale.z) + model_position.z
            };

            // Compute triangle AABB for culling
            Vector3 const tri_min = {
                glm::min(glm::min(v0.x, v1.x), v2.x),
                glm::min(glm::min(v0.y, v1.y), v2.y),
                glm::min(glm::min(v0.z, v1.z), v2.z)
            };
            Vector3 const tri_max = {
                glm::max(glm::max(v0.x, v1.x), v2.x),
                glm::max(glm::max(v0.y, v1.y), v2.y),
                glm::max(glm::max(v0.z, v1.z), v2.z)
            };

            // Cull triangle if player sphere doesn't intersect its AABB
            if (!math_aabb_sphere_intersection(tri_min, tri_max, player_position, player_radius * 2.0F)) {
                continue;
            }

            // Compute triangle normal
            Vector3 const edge1 = Vector3Subtract(v1, v0);
            Vector3 const edge2 = Vector3Subtract(v2, v0);
            Vector3 const normal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));

            // Floor detection: triangles with upward-facing normals
            F32 constexpr floor_normal_threshold = 0.7F;
            if (normal.y > floor_normal_threshold) {
                    // Test MULTIPLE points around player to handle triangle seams robustly
                    // This prevents falling through gaps at triangle boundaries
                    Vector3 test_points[5];
                    test_points[0] = (Vector3){player_position.x, player_position.y, player_position.z};  // Center
                    test_points[1] = (Vector3){player_position.x + (player_radius * 0.5F), player_position.y, player_position.z};
                    test_points[2] = (Vector3){player_position.x - (player_radius * 0.5F), player_position.y, player_position.z};
                    test_points[3] = (Vector3){player_position.x, player_position.y, player_position.z + (player_radius * 0.5F)};
                    test_points[4] = (Vector3){player_position.x, player_position.y, player_position.z - (player_radius * 0.5F)};

                    F32 const v0x = v0.x;
                    F32 const v0z = v0.z;
                    F32 const v1x = v1.x;
                    F32 const v1z = v1.z;
                    F32 const v2x = v2.x;
                    F32 const v2z = v2.z;

                    F32 const denom = ((v1z - v2z) * (v0x - v2x)) + ((v2x - v1x) * (v0z - v2z));
                    if (math_abs_f32(denom) < 0.000001F) { continue; }

                    for (Vector3 const &test_point : test_points) {
                        F32 const px = test_point.x;
                        F32 const pz = test_point.z;

                        // Barycentric coordinates for point-in-triangle test
                        F32 const w0 = ((v1z - v2z) * (px - v2x) + (v2x - v1x) * (pz - v2z)) / denom;
                        F32 const w1 = ((v2z - v0z) * (px - v2x) + (v0x - v2x) * (pz - v2z)) / denom;
                        F32 const w2 = 1.0F - w0 - w1;

                        // GENEROUS epsilon to handle seams - better to snap too much than fall through!
                        F32 constexpr edge_epsilon = 0.1F;
                        if (w0 >= -edge_epsilon && w1 >= -edge_epsilon && w2 >= -edge_epsilon) {
                            // Calculate floor height using barycentric interpolation
                            F32 const floor_y = (w0 * v0.y) + (w1 * v1.y) + (w2 * v2.y);

                            // Check snap distance
                            F32 const distance_from_player = math_abs_f32(player_position.y - floor_y);
                            F32 constexpr max_snap_distance = 12.0F;

                            if (distance_from_player < max_snap_distance) {
                                // Take closest floor to current position
                                if (distance_from_player < closest_floor_distance) {
                                    closest_floor_distance = distance_from_player;
                                    closest_floor_y = floor_y;
                                    found_floor = true;
                                }
                            }
                            break;  // Found floor with this test point, no need to test others
                        }
                    }
                }

                // Wall collision: triangles with more vertical normals
                if (normal.y < floor_normal_threshold) {
                    // Swept cylinder collision - prevents tunneling at high speeds
                    Vector3 const edges[3][2] = {
                        {v0, v1},
                        {v1, v2},
                        {v2, v0}
                    };

                    for (Vector3 const *edge : edges) {
                        Vector3 const edge_start = edge[0];
                        Vector3 const edge_end   = edge[1];

                        // Project to XZ plane
                        F32 const ex0 = edge_start.x;
                        F32 const ez0 = edge_start.z;
                        F32 const ex1 = edge_end.x;
                        F32 const ez1 = edge_end.z;

                        F32 const edge_dx     = ex1 - ex0;
                        F32 const edge_dz     = ez1 - ez0;
                        F32 const edge_len_sq = (edge_dx * edge_dx) + (edge_dz * edge_dz);

                        if (edge_len_sq < 0.0001F) { continue; }

                        // SWEPT COLLISION: Sample positions along movement path
                        // More samples for faster movement (prevents tunneling at high speeds)
                        SZ const num_samples = (is_moving && movement_length > player_radius * 2.0F) ? 3 : 2;

                        for (SZ sample_idx = 0; sample_idx < num_samples; ++sample_idx) {
                            F32 const t_sample     = (F32)sample_idx / (F32)(num_samples - 1);  // 0.0 to 1.0
                            Vector3 const test_pos = {
                                previous_position.x + (movement.x * t_sample),
                                previous_position.y + (movement.y * t_sample),
                                previous_position.z + (movement.z * t_sample)
                            };

                            // Find closest point on edge to test position
                            F32 const t         = ((test_pos.x - ex0) * edge_dx + (test_pos.z - ez0) * edge_dz) / edge_len_sq;
                            F32 const t_clamped = glm::clamp(t, 0.0F, 1.0F);

                            F32 const closest_x = ex0 + (t_clamped * edge_dx);
                            F32 const closest_z = ez0 + (t_clamped * edge_dz);

                            F32 const diff_x  = test_pos.x - closest_x;
                            F32 const diff_z  = test_pos.z - closest_z;
                            F32 const dist_sq = (diff_x * diff_x) + (diff_z * diff_z);

                            if (dist_sq < player_radius * player_radius && dist_sq > 0.0001F) {
                                F32 const dist        = math_sqrt_f32(dist_sq);
                                F32 const penetration = player_radius - dist;

                                if (penetration > 0.0F) {
                                    // Weight by position in sweep - current position gets most correction
                                    F32 const correction_factor = 0.5F + (0.5F * t_sample);
                                    wall_correction.x += (diff_x / dist) * penetration * correction_factor;
                                    wall_correction.z += (diff_z / dist) * penetration * correction_factor;
                                }
                            }
                        }
                    }
                }
        }
    }

    // Apply wall corrections (with damping for smoothness)
    F32 const correction_magnitude = math_sqrt_f32((wall_correction.x * wall_correction.x) + (wall_correction.z * wall_correction.z));
    if (correction_magnitude > 0.0001F) {
        // Clamp maximum correction for smoothness
        F32 constexpr max_correction_per_frame = player_radius * 1.5F;  // Increased for swept collision
        if (correction_magnitude > max_correction_per_frame) {
            F32 const scale = max_correction_per_frame / correction_magnitude;
            wall_correction.x *= scale;
            wall_correction.z *= scale;
        }
        g_world->player.camera3d.position.x += wall_correction.x;
        g_world->player.camera3d.position.z += wall_correction.z;
    }

    // Apply floor snapping
    if (found_floor) {
        g_world->player.camera3d.position.y = closest_floor_y + player_radius;
    }

    // Store position for next frame's swept collision (AFTER all corrections!)
    g_world->player.previous_position = g_world->player.camera3d.position;

    return found_floor;
}

F32 world_get_distance_to_player(Vector3 position) {
    return Vector3Distance(position, g_world->player.camera3d.position);
}

void world_randomly_rotate_entities() {
    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];

        // WE DO NOT WANT TO ROTATE PROPS.
        if (g_world->type[i] == ENTITY_TYPE_PROP) { continue; }

        F32 const rotation_amount = random_f32(0.0F, 360.0F);
        entity_add_rotation(i, rotation_amount);
    }
}

void world_dump_all_entities() {
    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];

        Vector3 const position = g_world->position[i];
        Vector3 const scale    = g_world->scale[i];
        F32 const rotation     = g_world->rotation[i];

        C8 const *type = entity_type_to_cstr(g_world->type[i]);
        lln("Entity_%u: %s (%s) at (%f, %f, %f) with scale (%f, %f, %f) and rotation %f",
            i, g_world->name[i], type, position.x, position.y, position.z, scale.x, scale.y, scale.z, rotation);
    }

    lln("active_entities: %u", g_world->active_ent_count);
    lln("max_generation:  %u", g_world->max_gen);
    lln("selected_entity: %u", g_world->selected_id);
}

void world_dump_all_entities_cb(void *data) {
    unused(data);
    world_dump_all_entities();
}

void world_set_overworld(ATerrain *terrain) {
    g_world = &g_world_state.overworld;
    g_world->base_terrain = terrain;
    if (!g_world_state.initialized) { world_init(); }
}

void world_set_dungeon(ATerrain *terrain) {
    g_world = &g_world_state.dungeon;
    g_world->base_terrain = terrain;
    if (!g_world_state.initialized) { world_init(); }
}

#define WORLD_RECORDER_FOLDER "state"
#define WORLD_RECORDER_BASE_FILE_NAME "world"
#define WORLD_RECORDER_FILE_EXTENSION "ws"
#define WORLD_RECORDER_MESSAGE_EVERY_N_MSEC 500.0F

// NOTE: We store the world recorder not in the world, but as a global variable to avoid copying it when saving/loading the world state.

WorldRecorder static i_rec = {};

void world_recorder_init() {
    world_recorder_clear();
}

void world_recorder_draw_2d_hud() {
    Vector2 const res    = render_get_render_resolution();
    Vector2 const center = {res.x / 2.0F, res.y / 4.0F};

    if (i_rec.record_world_state) {
        F32 const radius = ui_scale_x(0.5F);
        d2d_circle(center, radius, Fade(RED, 0.75F));
    } else if (i_rec.keep_looping) {
        F32 const size = ui_scale_x(1.0F);
        Vector2 const p1 = {center.x - (size / 3.0F), center.y - (size / 2.0F)};  // Top point
        Vector2 const p2 = {center.x - (size / 3.0F), center.y + (size / 2.0F)};  // Bottom point
        Vector2 const p3 = {center.x + (2.0F * size / 3.0F), center.y};           // Right point
        d2d_triangle(p1, p2, p3, Fade(GREEN, 0.75F));
    }
}

void world_recorder_update() {
    if (i_rec.world_state_go_back) {
        i_rec.world_state_go_back = false;

        // Return if nothing recorded yet.
        if (i_rec.record_world_state_cursor == 0) { return; }

        if (i_rec.world_state_cursor > 0) { i_rec.world_state_cursor--; }

        C8 const *file_path = TS("%s/%s_%zu.%s", WORLD_RECORDER_FOLDER, WORLD_RECORDER_BASE_FILE_NAME, i_rec.world_state_cursor, WORLD_RECORDER_FILE_EXTENSION)->c;
        world_recorder_load_state_to_file(file_path);
    }

    if (i_rec.world_state_go_forward) {
        i_rec.world_state_go_forward = false;

        // Return if nothing recorded yet.
        if (i_rec.record_world_state_cursor == 0) { return; }

        if (i_rec.world_state_cursor != i_rec.record_world_state_cursor - 1) { i_rec.world_state_cursor++; }

        C8 const *file_path = TS("%s/%s_%zu.%s", WORLD_RECORDER_FOLDER, WORLD_RECORDER_BASE_FILE_NAME, i_rec.world_state_cursor, WORLD_RECORDER_FILE_EXTENSION)->c;
        world_recorder_load_state_to_file(file_path);
    }

    if (i_rec.keep_looping) {
        if (i_rec.world_state_cursor == i_rec.record_world_state_cursor - 1) { i_rec.world_state_cursor = 0; }

        C8 const *file_path = TS("%s/%s_%zu.%s", WORLD_RECORDER_FOLDER, WORLD_RECORDER_BASE_FILE_NAME, i_rec.world_state_cursor, WORLD_RECORDER_FILE_EXTENSION)->c;
        world_recorder_load_state_to_file(file_path);

        i_rec.world_state_cursor++;
    }

    if (i_rec.record_world_state) {
        C8 const *file_path = TS("%s/%s_%zu.%s", WORLD_RECORDER_FOLDER, WORLD_RECORDER_BASE_FILE_NAME, i_rec.record_world_state_cursor, WORLD_RECORDER_FILE_EXTENSION)->c;
        world_recorder_save_state_to_file(file_path);

        i_rec.record_world_state_cursor++;
    }
}

void world_recorder_toggle_record_state() {
    i_rec.record_world_state = !i_rec.record_world_state;

    if (i_rec.keep_looping) { i_rec.keep_looping = false; }

    if (i_rec.record_world_state) {
        mi(TS("Recording starting from frame: %zu", i_rec.record_world_state_cursor)->c, RED);
    } else {
        mi(TS("Recorded %zu frames", i_rec.record_world_state_cursor)->c, GREEN);
    }
}

void world_recorder_toggle_loop_state() {
    // Abort if nothing has been recorded yet.
    if (i_rec.record_world_state_cursor == 0) {
        mi("Nothing to loop", RED);
        return;
    }

    i_rec.record_world_state = false;
    i_rec.keep_looping = !i_rec.keep_looping;

    mi(TS("Looping: %s", i_rec.keep_looping ? "ON" : "OFF")->c, i_rec.keep_looping ? GREEN : RED);
}

void world_recorder_backward_state() {
    if (i_rec.record_world_state) { world_recorder_toggle_record_state(); }

    i_rec.world_state_go_back = true;

    F32 const message_every_n_msec = WORLD_RECORDER_MESSAGE_EVERY_N_MSEC;
    F32 static time_passed         = message_every_n_msec;

    time_passed += time_get();
    if (time_passed > message_every_n_msec) {
        mi(TS("<< Frame: %zu", i_rec.world_state_cursor)->c, YELLOW);
        time_passed = 0.0F;
    }
}

void world_recorder_forward_state() {
    if (i_rec.record_world_state) { world_recorder_toggle_record_state(); }

    i_rec.world_state_go_forward = true;

    F32 const message_every_n_msec = WORLD_RECORDER_MESSAGE_EVERY_N_MSEC;
    F32 static time_passed         = message_every_n_msec;

    time_passed += time_get();
    if (time_passed > message_every_n_msec) {
        mi(TS(">> Frame: %zu", i_rec.world_state_cursor)->c, YELLOW);
        time_passed = 0.0F;
    }
}

void world_recorder_clear() {
    i_rec.record_world_state        = false;
    i_rec.record_world_state_cursor = 0;

    i_rec.world_state_go_back    = false;
    i_rec.world_state_go_forward = false;
    i_rec.world_state_cursor     = 0;

    i_rec.keep_looping = false;
}

void world_recorder_delete_recorded_state() {
    // Before we do anything, we make sure that the WORLD_RECORDER_FOLDER exists.
    // If it does not, we can safely return and do nothing.
    if (!DirectoryExists(WORLD_RECORDER_FOLDER "/")) {
        llw("World state folder does not exist: %s, nothing to delete", WORLD_RECORDER_FOLDER);
        return;
    }

    FilePathList const list = LoadDirectoryFiles(WORLD_RECORDER_FOLDER);

    if (list.count == 0) {
        mi("No world state files to delete", RED);
        return;
    }

    SZ deleted_files = 0;
    for (SZ i = 0; i < list.count; ++i) {
        C8 const *file_path = list.paths[i];
        llt("Deleting world state file: %s", file_path);
        S32 const result = remove(file_path);
        if (result != 0) {
            lle("Failed to delete world state file: %s", file_path);
            continue;
        }
        deleted_files++;
        world_recorder_clear();
    }

    mi(TS("Deleted %zu world state files", deleted_files)->c, RED);

    UnloadDirectoryFiles(list);
}

BOOL world_recorder_is_recording_state() {
    return i_rec.record_world_state;
}

BOOL world_recorder_is_looping_state() {
    return i_rec.keep_looping;
}

SZ world_recorder_get_record_cursor() {
    return i_rec.record_world_state_cursor;
}

SZ world_recorder_get_playback_cursor() {
    return i_rec.world_state_cursor;
}

SZ *world_recorder_get_playback_cursor_ptr() {
    return &i_rec.world_state_cursor;
}

void world_recorder_save_state_to_file(C8 const *file_path) {
    llt("Saving world state to file: %s", file_path);

    if (!DirectoryExists(WORLD_RECORDER_FOLDER "/")) {
        llt("World state folder does not exist, creating it: %s", WORLD_RECORDER_FOLDER);
        if (mkdir(WORLD_RECORDER_FOLDER, 0777) != 0) {
            lle("Failed to create world state folder: %s", WORLD_RECORDER_FOLDER);
            return;
        }
    }

    FILE *f = fopen(file_path, "wb");
    if (!f) {
        lle("Failed to open file for writing: %s", file_path);
        return;
    }

    SZ const num_elements         = 1;
    SZ const num_elements_written = fwrite(g_world, sizeof(World), num_elements, f);
    if (num_elements_written != num_elements) {
        lle("Failed to write world state to file: %s", file_path);
        // We want to close either way.
    }

    if (fclose(f) != 0) { lle("Failed to close file: %s, %s", file_path, strerror(errno)); }
}

void world_recorder_load_state_to_file(C8 const *file_path) {
    llt("Loading world state from file: %s", file_path);

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        lle("Failed to open file for reading: %s", file_path);
        return;
    }

    SZ const num_elements = 1;
    if (fread(g_world, sizeof(World), num_elements, file) != num_elements) {
        lle("Failed to read world state from file: %s", file_path);
        // We want to close either way.
    }

    if (fclose(file) != 0) { lle("Failed to close file: %s, %s", file_path, strerror(errno)); }
}

SZ world_recorder_get_state_size_bytes() {
    return sizeof(World);
}

SZ world_recorder_get_total_recorded_size_bytes() {
    return sizeof(World) * i_rec.record_world_state_cursor;
}

SZ world_recorder_get_actual_disk_usage_bytes() {
    if (!DirectoryExists(WORLD_RECORDER_FOLDER "/")) { return 0; }

    FilePathList const list = LoadDirectoryFiles(WORLD_RECORDER_FOLDER);
    SZ total_size           = 0;

    for (SZ i = 0; i < list.count; ++i) {
        C8 const *file_path = list.paths[i];
        // Only count .ws files to avoid counting other files in the directory
        if (ou_strstr(file_path, "." WORLD_RECORDER_FILE_EXTENSION) != nullptr) {
            struct stat st;
            if (stat(file_path, &st) == 0) { total_size += (SZ)st.st_size; }
        }
    }

    UnloadDirectoryFiles(list);
    return total_size;
}

void world_update_follower_cache() {
    if (!g_world->follower_cache.dirty) { return; }

    for (U8 &count : g_world->follower_cache.follower_counts) { count = 0; }

    for (SZ cell_idx = 0; cell_idx < (SZ)GRID_TOTAL_CELLS; ++cell_idx) {
        GridCell *cell = grid_get_cell_by_index(cell_idx);
        if (!cell || cell->entity_count == 0) { continue; }

        for (SZ i = 0; i < cell->entity_count; ++i) {
            EID const entity_id = cell->entities[i];
            if (!ENTITY_HAS_FLAG(g_world->flags[entity_id], ENTITY_FLAG_IN_USE)) { continue; }
            if (g_world->type[entity_id] != ENTITY_TYPE_NPC) { continue; }

            EntityBehaviorController const *behavior = &g_world->actor[entity_id].behavior;
            if (behavior->target_id != INVALID_EID && g_world->generation[behavior->target_id] == behavior->target_gen) {
                g_world->follower_cache.follower_counts[behavior->target_id]++;
            }
        }
    }

    g_world->follower_cache.dirty = false;
}

S32 world_followed_by_count(EID id) {
    world_update_follower_cache();
    return (S32)g_world->follower_cache.follower_counts[id];
}

void world_mark_follower_cache_dirty() {
    g_world->follower_cache.dirty = true;
}

void world_target_tracker_add(EID target_id, EID actor_id) {
    if (target_id == INVALID_EID || target_id >= WORLD_MAX_ENTITIES) { return; }

    auto *tracker = &g_world->target_trackers[target_id];

    // Check if already tracking
    for (U8 i = 0; i < tracker->count; i++) {
        if (tracker->actors[i] == actor_id) { return; }
    }

    // Add if there's space
    if (tracker->count < MAX_ACTORS_PER_TARGET) {
        tracker->actors[tracker->count] = actor_id;
        tracker->count++;
    }
}

void world_target_tracker_remove(EID target_id, EID actor_id) {
    if (target_id == INVALID_EID || target_id >= WORLD_MAX_ENTITIES) { return; }

    auto *tracker = &g_world->target_trackers[target_id];

    // Find and remove
    for (U8 i = 0; i < tracker->count; i++) {
        if (tracker->actors[i] == actor_id) {
            // Move last element to this position
            tracker->count--;
            tracker->actors[i] = tracker->actors[tracker->count];
            return;
        }
    }
}

void world_notify_actors_target_destroyed(EID destroyed_target_id) {
    if (destroyed_target_id == INVALID_EID || destroyed_target_id >= WORLD_MAX_ENTITIES) { return; }

    String const *reason = TS("%04d destroyed by others", destroyed_target_id);
    auto *tracker        = &g_world->target_trackers[destroyed_target_id];

    for (U8 i = 0; i < tracker->count; i++) {
        EID const actor_id = tracker->actors[i];

        if (!ENTITY_HAS_FLAG(g_world->flags[actor_id], ENTITY_FLAG_IN_USE)) { continue; }
        if (g_world->type[actor_id] != ENTITY_TYPE_NPC)                     { continue; }

        entity_actor_clear_actor_target(actor_id);
        EntityActor *actor = &g_world->actor[actor_id];

        switch (actor->behavior.state) {
            case ENTITY_BEHAVIOR_STATE_GOING_TO_TARGET:
            case ENTITY_BEHAVIOR_STATE_HARVESTING_TARGET: {
                entity_actor_search_and_transition_to_target(actor_id, reason->c);
            } break;

            case ENTITY_BEHAVIOR_STATE_DELIVERING_TO_LUMBERYARD:
            case ENTITY_BEHAVIOR_STATE_ATTACKING_NPC: {
                entity_actor_behavior_transition_to_state(actor_id, ENTITY_BEHAVIOR_STATE_IDLE, reason->c);
            } break;

            default: {
                break;
            }
        }
    }

    // Clear the tracker
    tracker->count = 0;
}
