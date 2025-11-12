#include "world.hpp"
#include "asset.hpp"
#include "audio.hpp"
#include "color.hpp"
#include "cvar.hpp"
#include "dungeon.hpp"
#include "edit.hpp"
#include "log.hpp"
#include "map.hpp"
#include "memory.hpp"
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
AnimationBoneData *g_animation_bones = nullptr;

void world_init() {
    // Allocate both worlds using permanent arena
    g_world_state.overworld = mmpa(World*, sizeof(World));
    g_world_state.dungeon = mmpa(World*, sizeof(World));

    // Allocate bone matrix buffer (NOT saved by recorder)
    g_animation_bones = mmpa(AnimationBoneData*, sizeof(AnimationBoneData) * WORLD_MAX_ENTITIES);

    // Reset both worlds and default on start on overworld
    g_world = g_world_state.dungeon;
    world_reset();

    g_world = g_world_state.overworld;
    world_reset();

    world_recorder_init();
    entity_init();
    grid_init({A_TERRAIN_DEFAULT_SIZE, A_TERRAIN_DEFAULT_SIZE}); // TODO: We need to make sure that we set the proper size. Right now we are defaulting.
    g_world_state.initialized = true;

    player_init();
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
        g_world->model_name_hash[i] = 0;
        g_world->tint[i]           = {};
        g_world->generation[i]     = 0;
        g_world->talker[i]         = {};
        g_world->building[i]       = {};
        g_world->name[i][0]        = '\0';
    }

    g_world->active_ent_count      = 0;
    g_world->active_entity_count   = 0;
    g_world->max_gen               = 0;
    g_world->selected_entity_count = 0;

    grid_clear();
}

void world_update(F32 dt, F32 dtu) {
    g_render.visible_vertex_count = 0;

    world_recorder_update();  // WARN: This needs to happen before anything that changes the world.
    grid_populate();
    edit_update(dt, dtu);
    c3d_update_frustum();

    for (U32 &count : g_world->entity_type_counts) { count = 0; }

    // Use active entities from last frame
    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (!ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_IN_USE)) { continue; }  // Safety check for entities that died during edit update

        g_world->lifetime[i] += dt;
        g_world->entity_type_counts[g_world->type[i]]++;

        // TODO: Do we really need to check here if they are instanced?
        c3d_is_obb_in_frustum(g_world->obb[i]) ? ENTITY_SET_FLAG(g_world->flags[i], ENTITY_FLAG_IN_FRUSTUM)
                                              : ENTITY_CLEAR_FLAG(g_world->flags[i], ENTITY_FLAG_IN_FRUSTUM);

        if (ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_IN_FRUSTUM)) { g_render.visible_vertex_count += asset_get_model_by_hash(g_world->model_name_hash[i])->vertex_count; }
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

    // TODO: Multithread or compute shader? Compute shader would mean that we cannot run this on modern Apple devices though
    // Update all entity animations
    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const id = g_world->active_entities[idx];

        if (!g_world->animation[id].has_animations) { continue; }
        if (!g_world->animation[id].anim_playing)   { continue; }

        AModel *model      = asset_get_model_by_hash(g_world->model_name_hash[id]);
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
        math_compute_entity_bone_matrices(id);
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

        // Check occlusion by dungeon walls (only in dungeon scene)
        if (g_scenes.current_scene_type == SCENE_DUNGEON) {
            if (dungeon_is_entity_occluded(i)) { continue; }
        }

        if (g_world->type[i] == ENTITY_TYPE_NPC) {
            entity_actor_draw_2d_hud(i);
            d2d_healthbar_batched(i);  // Collect healthbar data for batched rendering
        }
    }

    // Draw all healthbars in a single batched draw call
    d2d_healthbar_draw_batched();

    edit_draw_2d_hud();
}

void world_draw_2d_dbg() {
    if (!c_debug__enabled) { return; }

    grid_draw_2d_dbg();

    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (!ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_IN_FRUSTUM)) { continue; }

        // Check occlusion by dungeon walls (only in dungeon scene)
        if (g_scenes.current_scene_type == SCENE_DUNGEON) {
            if (dungeon_is_entity_occluded(i)) { continue; }
        }

        // Check if entity is in selection
        BOOL is_selected = false;
        for (SZ sel_idx = 0; sel_idx < g_world->selected_entity_count; ++sel_idx) {
            if (g_world->selected_entities[sel_idx] == i) {
                is_selected = true;
                break;
            }
        }

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
#define BACKPACK_OFFSET_BACKWARD 1.00F
// How far above the actor's base position
#define BACKPACK_OFFSET_UPWARD -0.3F
// Scale of the backpack relative to actor scale
#define BACKPACK_MAX_SCALE  0.2F
// Minimum number of instances to use instanced rendering (avoids overhead for single entities)
#define MIN_INSTANCE_COUNT 2

// Instanced rendering: map from model name to array of entity IDs
MAP_DECLARE(InstanceGroupMap, U32, EIDArray, MAP_HASH_U32, MAP_EQUAL_U32);

struct AnimationStateKey {
    U32 model_hash;
    U32 anim_index;
    S32 bone_count;
    BOOL is_blending;
    U32 prev_anim_index;
};

static inline U32 animation_state_key_hash(AnimationStateKey key) {
    U32 hash = 2166136261u;
    hash ^= key.model_hash;
    hash *= 16777619u;
    hash ^= key.anim_index;
    hash *= 16777619u;
    hash ^= (U32)key.bone_count;
    hash *= 16777619u;
    hash ^= (U32)key.is_blending;
    hash *= 16777619u;
    hash ^= key.prev_anim_index;
    hash *= 16777619u;
    return hash;
}

static inline BOOL animation_state_key_equal(AnimationStateKey a, AnimationStateKey b) {
    return a.model_hash == b.model_hash &&
           a.anim_index == b.anim_index &&
           a.bone_count == b.bone_count &&
           a.is_blending == b.is_blending &&
           a.prev_anim_index == b.prev_anim_index;
}

MAP_DECLARE(AnimatedInstanceGroupMap, AnimationStateKey, EIDArray, animation_state_key_hash, animation_state_key_equal);
void world_draw_3d_sketch() {
    F32 const bp_base_scale = BACKPACK_MAX_SCALE*0.5F;

    // Group static entities by model name for instanced rendering
    InstanceGroupMap instance_groups;
    InstanceGroupMap_init(&instance_groups, MEMORY_TYPE_ARENA_TRANSIENT, 32);

    // Group animated entities by animation state for instanced rendering
    AnimatedInstanceGroupMap animated_instance_groups;
    AnimatedInstanceGroupMap_init(&animated_instance_groups, MEMORY_TYPE_ARENA_TRANSIENT, 32);

    // Collect backpack instances for batch rendering
    MatrixArray backpack_transforms;
    ColorArray backpack_tints;
    array_init(MEMORY_TYPE_ARENA_TRANSIENT, &backpack_transforms, 1024);
    array_init(MEMORY_TYPE_ARENA_TRANSIENT, &backpack_tints, 1024);

    // First pass: Group entities by rendering state
    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (!ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_IN_FRUSTUM)) { continue; }

        // Check occlusion by dungeon walls (only in dungeon scene)
        if (g_scenes.current_scene_type == SCENE_DUNGEON) {
            if (dungeon_is_entity_occluded(i)) { continue; }
        }

        if (g_world->animation[i].has_animations) {
            AnimationStateKey key = {
                .model_hash = g_world->model_name_hash[i],
                .anim_index = g_world->animation[i].anim_index,
                .bone_count = g_world->animation[i].bone_count,
                .is_blending = g_world->animation[i].is_blending,
                .prev_anim_index = g_world->animation[i].prev_anim_index
            };

            EIDArray *group = AnimatedInstanceGroupMap_get(&animated_instance_groups, key);
            if (!group) {
                EIDArray new_group = {};
                array_init(MEMORY_TYPE_ARENA_TRANSIENT, &new_group, 64);
                AnimatedInstanceGroupMap_insert(&animated_instance_groups, key, new_group);
                group = AnimatedInstanceGroupMap_get(&animated_instance_groups, key);
            }

            if (group) {
                array_push(group, i);
            }
        } else {
            U32 const model_name_hash = asset_get_model_by_hash(g_world->model_name_hash[i])->header.name_hash;
            EIDArray *group = InstanceGroupMap_get(&instance_groups, model_name_hash);
            if (!group) {
                EIDArray new_group = {};
                array_init(MEMORY_TYPE_ARENA_TRANSIENT, &new_group, 64);
                InstanceGroupMap_insert(&instance_groups, model_name_hash, new_group);
                group = InstanceGroupMap_get(&instance_groups, model_name_hash);
            }

            if (group) {
                array_push(group, i);
            }
        }

        // Draw carried resources on actor's back
        if (ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_ACTOR)) {
            SZ const wood_count = g_world->actor[i].behavior.wood_count;
            if (wood_count > 0) {
                F32 const rotation = g_world->rotation[i];
                Vector3 scale = g_world->scale[i];
                Vector3 backpack_pos;

                // Attach to bone - skip if bone not found
                C8 const *bone_name = "socket_hat";
                if (!math_get_bone_world_position_by_name(i, bone_name, &backpack_pos)) {
                    llw("Could not find bone to attach backpack");
                    continue;  // Skip this backpack if we can't find the bone
                }

                // Offset backpack backwards and down
                F32 const rotation_rad = rotation * DEG2RAD;
                Vector3 forward = {math_sin_f32(rotation_rad), 0.0F, math_cos_f32(rotation_rad)};
                Vector3 up = {0.0F, 1.0F, 0.0F};
                Vector3 backpack_offset = Vector3Scale(forward, -BACKPACK_OFFSET_BACKWARD * scale.z);  // Behind
                backpack_offset = Vector3Add(backpack_offset, Vector3Scale(up, BACKPACK_OFFSET_UPWARD * scale.y));  // Up (inverted to go down)
                backpack_pos = Vector3Add(backpack_pos, backpack_offset);

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

                // Scale backpack with entity scale
                Vector3 backpack_scale = Vector3Scale(scale, backpack_scale_multiplier);
                Color wood_color       = {139, 90, 43, 255};

                // Build transform matrix for this backpack instance with tilt
                Matrix mat_scale = MatrixScale(backpack_scale.x, backpack_scale.y, backpack_scale.z);

                // Tilt backpack diagonally towards the head (pitch forward)
                F32 const tilt_angle = 5.0F; // Degrees to tilt towards head
                Matrix mat_tilt = MatrixRotate((Vector3){1, 0, 0}, tilt_angle * DEG2RAD); // Pitch
                Matrix mat_rot_y = MatrixRotate((Vector3){0, 1, 0}, rotation * DEG2RAD); // Yaw
                Matrix mat_rot = MatrixMultiply(mat_tilt, mat_rot_y);

                Matrix mat_trans = MatrixTranslate(backpack_pos.x, backpack_pos.y, backpack_pos.z);
                Matrix transform = MatrixMultiply(MatrixMultiply(mat_scale, mat_rot), mat_trans);

                array_push(&backpack_transforms, transform);
                array_push(&backpack_tints, wood_color);
            }
        }
    }

    // Second pass: Batch render animated entity groups
    AnimationStateKey anim_key = {};
    EIDArray anim_group = {};
    MAP_EACH(&animated_instance_groups, anim_key, anim_group) {
        // Validate group has data and count before processing
        if (!anim_group.data || anim_group.count == 0) { continue; }

        if (anim_group.count < MIN_INSTANCE_COUNT) {
            // Not worth instancing for single/few entities - use regular rendering
            for (SZ j = 0; j < anim_group.count; ++j) {
                EID const i = anim_group.data[j];
                S32 is_selected = world_is_entity_selected(i) ? 1 : 0;
                SetShaderValue(g_render.model_shader.shader->base, g_render.model_shader.is_selected_loc, &is_selected, SHADER_UNIFORM_INT);

                d3d_model_animated_by_hash(
                    g_world->model_name_hash[i],
                    g_world->position[i],
                    g_world->rotation[i],
                    g_world->scale[i],
                    g_world->tint[i],
                    g_animation_bones[i].bone_matrices,
                    g_world->animation[i].bone_count
                );
            }
        } else {
            // Build transform and color arrays, split by selection state
            MatrixArray transforms_selected;
            ColorArray tints_selected;
            MatrixArray transforms_unselected;
            ColorArray tints_unselected;
            array_init(MEMORY_TYPE_ARENA_TRANSIENT, &transforms_selected, anim_group.count);
            array_init(MEMORY_TYPE_ARENA_TRANSIENT, &tints_selected, anim_group.count);
            array_init(MEMORY_TYPE_ARENA_TRANSIENT, &transforms_unselected, anim_group.count);
            array_init(MEMORY_TYPE_ARENA_TRANSIENT, &tints_unselected, anim_group.count);

            // Get bone matrices from first entity in group (they all share the same animation state)
            EID const first_entity = anim_group.data[0];
            Matrix *bone_matrices = g_animation_bones[first_entity].bone_matrices;
            S32 bone_count = g_world->animation[first_entity].bone_count;

            for (SZ j = 0; j < anim_group.count; ++j) {
                EID const i = anim_group.data[j];
                Matrix mat_scale = MatrixScale(g_world->scale[i].x, g_world->scale[i].y, g_world->scale[i].z);
                Matrix mat_rot = MatrixRotate((Vector3){0, 1, 0}, g_world->rotation[i] * DEG2RAD);
                Matrix mat_trans = MatrixTranslate(g_world->position[i].x, g_world->position[i].y, g_world->position[i].z);
                Matrix transform = MatrixMultiply(MatrixMultiply(mat_scale, mat_rot), mat_trans);

                if (world_is_entity_selected(i)) {
                    array_push(&transforms_selected, transform);
                    array_push(&tints_selected, g_world->tint[i]);
                } else {
                    array_push(&transforms_unselected, transform);
                    array_push(&tints_unselected, g_world->tint[i]);
                }
            }

            // Draw non-selected animated instances (with NULL check)
            if (transforms_unselected.count > 0 && transforms_unselected.data && tints_unselected.data) {
                S32 is_selected = 0;
                SetShaderValue(g_render.model_animated_instanced_shader.shader->base, g_render.model_animated_instanced_shader.is_selected_loc, &is_selected, SHADER_UNIFORM_INT);
                d3d_model_animated_instanced_by_hash(anim_key.model_hash, transforms_unselected.data, tints_unselected.data, transforms_unselected.count, bone_matrices, bone_count);
            }

            // Draw selected animated instances (with NULL check)
            if (transforms_selected.count > 0 && transforms_selected.data && tints_selected.data) {
                S32 is_selected = 1;
                SetShaderValue(g_render.model_animated_instanced_shader.shader->base, g_render.model_animated_instanced_shader.is_selected_loc, &is_selected, SHADER_UNIFORM_INT);
                d3d_model_animated_instanced_by_hash(anim_key.model_hash, transforms_selected.data, tints_selected.data, transforms_selected.count, bone_matrices, bone_count);
            }
        }
    }

    // Third pass: Batch render static entity groups
    U32 model_name_hash = 0;
    EIDArray group = {};
    MAP_EACH(&instance_groups, model_name_hash, group) {
        // Validate group has data and count before processing
        if (!group.data || group.count == 0) { continue; }

        if (group.count < MIN_INSTANCE_COUNT) {
            // Not worth instancing for single/few entities - use regular rendering
            for (SZ j = 0; j < group.count; ++j) {
                EID const i = group.data[j];
                S32 is_selected = world_is_entity_selected(i) ? 1 : 0;
                SetShaderValue(g_render.model_shader.shader->base, g_render.model_shader.is_selected_loc, &is_selected, SHADER_UNIFORM_INT);

                d3d_model_by_hash(
                    model_name_hash,
                    g_world->position[i],
                    g_world->rotation[i],
                    g_world->scale[i],
                    g_world->tint[i]
                );
            }
        } else {
            // Build transform and color arrays, split by selection state
            MatrixArray transforms_selected;
            ColorArray tints_selected;
            MatrixArray transforms_unselected;
            ColorArray tints_unselected;
            array_init(MEMORY_TYPE_ARENA_TRANSIENT, &transforms_selected, group.count);
            array_init(MEMORY_TYPE_ARENA_TRANSIENT, &tints_selected, group.count);
            array_init(MEMORY_TYPE_ARENA_TRANSIENT, &transforms_unselected, group.count);
            array_init(MEMORY_TYPE_ARENA_TRANSIENT, &tints_unselected, group.count);

            for (SZ j = 0; j < group.count; ++j) {
                EID const i = group.data[j];
                Matrix mat_scale = MatrixScale(g_world->scale[i].x, g_world->scale[i].y, g_world->scale[i].z);
                Matrix mat_rot = MatrixRotate((Vector3){0, 1, 0}, g_world->rotation[i] * DEG2RAD);
                Matrix mat_trans = MatrixTranslate(g_world->position[i].x, g_world->position[i].y, g_world->position[i].z);
                Matrix transform = MatrixMultiply(MatrixMultiply(mat_scale, mat_rot), mat_trans);

                if (world_is_entity_selected(i)) {
                    array_push(&transforms_selected, transform);
                    array_push(&tints_selected, g_world->tint[i]);
                } else {
                    array_push(&transforms_unselected, transform);
                    array_push(&tints_unselected, g_world->tint[i]);
                }
            }

            // Draw non-selected instances (with NULL check)
            if (transforms_unselected.count > 0 && transforms_unselected.data && tints_unselected.data) {
                S32 is_selected = 0;
                SetShaderValue(g_render.model_instanced_shader.shader->base, g_render.model_instanced_shader.is_selected_loc, &is_selected, SHADER_UNIFORM_INT);
                d3d_model_instanced_by_hash(model_name_hash, transforms_unselected.data, tints_unselected.data, transforms_unselected.count);
            }

            // Draw selected instances (with NULL check)
            if (transforms_selected.count > 0 && transforms_selected.data && tints_selected.data) {
                S32 is_selected = 1;
                SetShaderValue(g_render.model_instanced_shader.shader->base, g_render.model_instanced_shader.is_selected_loc, &is_selected, SHADER_UNIFORM_INT);
                d3d_model_instanced_by_hash(model_name_hash, transforms_selected.data, tints_selected.data, transforms_selected.count);
            }
        }
    }

    // Fourth pass: Batch render all backpacks
    if (backpack_transforms.count > 0) {
        d3d_model_instanced("wood.glb", backpack_transforms.data, backpack_tints.data, backpack_transforms.count);
    }

    // Reset isSelected to 0 after entity rendering
    S32 is_selected = 0;
    SetShaderValue(g_render.model_shader.shader->base, g_render.model_shader.is_selected_loc, &is_selected, SHADER_UNIFORM_INT);
    SetShaderValue(g_render.model_instanced_shader.shader->base, g_render.model_instanced_shader.is_selected_loc, &is_selected, SHADER_UNIFORM_INT);
    SetShaderValue(g_render.model_animated_instanced_shader.shader->base, g_render.model_animated_instanced_shader.is_selected_loc, &is_selected, SHADER_UNIFORM_INT);
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

        // Check occlusion by dungeon walls (only in dungeon scene)
        if (g_scenes.current_scene_type == SCENE_DUNGEON) {
            if (dungeon_is_entity_occluded(i)) { continue; }
        }

        F32 gizmos_alpha       = 1.0F;
        BOOL const is_selected = world_is_entity_selected(i);
        Color const obb_color  = is_selected ? color_sync_blinking_regular(SALMON, PERSIMMON) : entity_type_to_color(g_world->type[i]);

        // We only do this fading by distance if the entity is not selected.
        if (!is_selected) {
            // If the target is more than X units, we can fade the gizmos. This is to avoid cluttering the screen.
            F32 const dist_till_no_fade = 75.0F;
            F32 const dist_at_full_fade = 150.0F;
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

        // Draw the radius info
        if (c_debug__radius_info && gizmos_alpha > 0.0F) {
            d3d_sphere(g_world->position[i], g_world->radius[i], Fade(MAGENTA, 0.35F * gizmos_alpha));
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

    // Clear previous selection and select this entity
    g_world->selected_entity_count = 0;
    g_world->selected_entities[g_world->selected_entity_count++] = id;

    audio_play(ACG_SFX, "selected_0.ogg");
}

BOOL world_is_entity_selected(EID id) {
    for (SZ i = 0; i < g_world->selected_entity_count; ++i) {
        if (g_world->selected_entities[i] == id) {
            return true;
        }
    }
    return false;
}

void world_vegetation_collision() {
    // INFO: This stupid func (i_check_vegetation_collision at the time) is stupid and all but it is what brought us the first element of what we
    // would call a game. Is it corny that I marked this spot immediately after trying the change to the scene? OOOOOOOOOH BABY!
    // NOTE: This is me after a few months(?). I don't understand the 2nd half of the sentence. I think I was trying to be funny. I don't know.
    // NOTE: Now optimized to use grid-based spatial queries instead of brute-force O(n) loop.

    Vector3 const player_position  = g_player.cameras[g_scenes.current_scene_type].position;
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

F32 world_get_distance_to_player(Vector3 position) {
    return Vector3Distance(position, g_player.cameras[g_scenes.current_scene_type].position);
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
}

void world_dump_all_entities_cb(void *data) {
    unused(data);
    world_dump_all_entities();
}

void world_set_overworld(ATerrain *terrain) {
    g_world = g_world_state.overworld;
    g_world->base_terrain = terrain;
    if (!g_world_state.initialized) { world_init(); }
}

void world_set_dungeon(ATerrain *terrain) {
    g_world = g_world_state.dungeon;
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

    // Recompute bone matrices for all animated entities
    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const id = g_world->active_entities[idx];
        if (!g_world->animation[id].has_animations) { continue; }
        if (!g_world->animation[id].anim_playing)   { continue; }
        math_compute_entity_bone_matrices(id);
    }
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
        if (!cell) { continue; }

        SZ const count = cell->count_per_type[ENTITY_TYPE_NPC];
        for (SZ i = 0; i < count; ++i) {
            EID const entity_id = cell->entities_by_type[ENTITY_TYPE_NPC][i];
            if (!ENTITY_HAS_FLAG(g_world->flags[entity_id], ENTITY_FLAG_IN_USE)) { continue; }

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
