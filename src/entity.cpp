#include "entity.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "audio.hpp"
#include "color.hpp"
#include "entity_actor.hpp"
#include "grid.hpp"
#include "input.hpp"
#include "log.hpp"
#include "math.hpp"
#include "particles_3d.hpp"
#include "render.hpp"
#include "std.hpp"
#include "string.hpp"
#include "word.hpp"
#include "world.hpp"

#include <raymath.h>
#include <glm/gtc/type_ptr.hpp>

Color static const i_entity_type_color[ENTITY_TYPE_COUNT] = {
    GRAY,    // NONE
    YELLOW,  // NPC
    GREEN,   // VEGETATION
    ORANGE,  // BUILDING_LUMBERYARD
    CYAN,    // PROP
};

C8 static const *i_entity_type_strings[ENTITY_TYPE_COUNT] = {
    "NONE", "NPC", "VEGETATION", "LUMBERYARD", "PROP",
};

void static i_update_obb_extents_and_center(EID id) {
    Vector3 const scale          = g_world->scale[id];
    BoundingBox const model_bbox = asset_get_model(g_world->model_name[id])->bb;
    Vector3 const min            = Vector3Transform(model_bbox.min, MatrixScale(scale.x, scale.y, scale.z));
    Vector3 const max            = Vector3Transform(model_bbox.max, MatrixScale(scale.x, scale.y, scale.z));

    g_world->obb[id].extents.x = (max.x - min.x) * 0.5F;
    g_world->obb[id].extents.y = (max.y - min.y) * 0.5F;
    g_world->obb[id].extents.z = (max.z - min.z) * 0.5F;

    g_world->radius[id] = glm::max(g_world->obb[id].extents.x, g_world->obb[id].extents.z);

    Vector3 center_offset   = {(min.x + max.x) * 0.5F, (min.y + max.y) * 0.5F, (min.z + max.z) * 0.5F};
    Matrix const transform  = MatrixRotateY(g_world->rotation[id] * DEG2RAD);
    center_offset           = Vector3Transform(center_offset, transform);
    g_world->obb[id].center = Vector3Add(g_world->position[id], center_offset);
}

void entity_init() {
    entity_actor_init();
    entity_building_init();
}

EID entity_create(EntityType type, C8 const *name, Vector3 position, F32 rotation, Vector3 scale, Color tint, C8 const *model_name) {
    EID id = INVALID_EID;

    // First, find an available entity ID
    for (EID i = 0; i < WORLD_MAX_ENTITIES; ++i) {
        if (!ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_IN_USE)) {
            id = i;
            break;
        }
    }

    // If no free entity slots are available, return error
    if (id == INVALID_EID) {
        llw("Failed to create entity, no more space in world");
        return id;
    }

    SZ const name_length = ou_strlen(name);
    if (name_length >= ENTITY_NAME_MAX_LENGTH - 1) {
        lle("Failed to create entity, name has too many characters (Max: %d, Current: %zu, Name: %s)", ENTITY_NAME_MAX_LENGTH, name_length, name);
        return id;
    }
    if (name_length == 0) { name = word_generate_name()->c; }  // Generate name if none given

    ou_strncpy(g_world->name[id], name, ENTITY_NAME_MAX_LENGTH - 1);
    g_world->name[id][ENTITY_NAME_MAX_LENGTH - 1] = '\0';

    ENTITY_SET_FLAG(g_world->flags[id], ENTITY_FLAG_IN_USE);
    g_world->generation[id]++;
    g_world->active_ent_count++;
    ou_strncpy(g_world->model_name[id], model_name, A_NAME_MAX_LENGTH);

    g_world->type[id]           = type;
    g_world->lifetime[id]       = 0.0F;
    g_world->position[id]       = position;
    g_world->rotation[id]       = rotation;
    g_world->scale[id]          = scale;
    g_world->original_scale[id] = scale;
    g_world->tint[id]           = tint;
    g_world->max_gen            = glm::max(g_world->max_gen, g_world->generation[id]);

    // Health
    g_world->health[id].max     = 150;
    g_world->health[id].current = g_world->health[id].max;

    // Initialize behavior controller
    g_world->actor[id].behavior.state              = ENTITY_BEHAVIOR_STATE_IDLE;
    g_world->actor[id].behavior.state_reason[0]    = '\0';
    g_world->actor[id].behavior.target_entity_type = ENTITY_TYPE_NONE;
    g_world->actor[id].behavior.target_position    = {0, 0, 0};
    g_world->actor[id].behavior.target_id          = INVALID_EID;
    g_world->actor[id].behavior.target_gen         = 0;
    g_world->actor[id].behavior.action_timer       = 0.0F;
    g_world->actor[id].behavior.wood_count         = 0;

    // Initialize movement controller
    g_world->actor[id].movement.state               = ENTITY_MOVEMENT_STATE_IDLE;
    g_world->actor[id].movement.speed               = 10.0F;
    g_world->actor[id].movement.current_speed       = 0.0F;
    g_world->actor[id].movement.acceleration        = 25.0F;
    g_world->actor[id].movement.velocity            = {0, 0, 0};
    g_world->actor[id].movement.separation_force    = {0, 0, 0};
    g_world->actor[id].movement.last_position       = position;
    g_world->actor[id].movement.stuck_timer         = 0.0F;
    g_world->actor[id].movement.target_offset_angle = 0.0F;
    g_world->actor[id].movement.use_target_offset   = false;
    g_world->actor[id].movement.goal_type           = ENTITY_MOVEMENT_GOAL_NONE;
    g_world->actor[id].movement.target_position     = {0, 0, 0};
    g_world->actor[id].movement.target_angle        = 0.0F;
    g_world->actor[id].movement.target_id           = INVALID_EID;
    g_world->actor[id].movement.target_gen          = 0;
    g_world->actor[id].movement.goal_completed      = true;
    g_world->actor[id].movement.goal_failed         = false;
    g_world->actor[id].movement.turn_start_angle    = 0.0F;
    g_world->actor[id].movement.turn_target_angle   = 0.0F;
    g_world->actor[id].movement.turn_duration       = 0.0F;
    g_world->actor[id].movement.turn_elapsed        = 0.0F;
    g_world->actor[id].movement.turn_ease_type      = EASE_LINEAR;

    entity_set_position(id, position);
    entity_set_rotation(id, rotation);
    entity_set_scale(id, scale);

    grid_add_entity(id, type, g_world->position[id]);

    // Initialize animation state
    AModel *model = asset_get_model(model_name);
    g_world->animation[id].has_animations = (model && model->has_animations);
    g_world->animation[id].bone_count     = (model ? model->base.boneCount : 0);
    // Determine animation FPS based on format (raylib uses ~60 FPS for GLTF/M3D, no standard for others)
    // Default to 60 FPS as per raylib's GLTF_ANIMDELAY/M3D_ANIMDELAY (17ms ≈ 58.8 FPS)
    g_world->animation[id].anim_fps     = 60.0F;
    g_world->animation[id].anim_index   = 0;
    g_world->animation[id].anim_frame   = 0;
    g_world->animation[id].anim_time    = 0.0F;
    g_world->animation[id].anim_speed   = 1.0F;
    g_world->animation[id].anim_loop    = true;
    g_world->animation[id].anim_playing = false;

    // Initialize blending state
    g_world->animation[id].is_blending      = false;
    g_world->animation[id].blend_time       = 0.0F;
    g_world->animation[id].blend_duration   = 0.2F;
    g_world->animation[id].prev_anim_index  = 0;
    g_world->animation[id].prev_anim_frame  = 0;

    // Initialize bone matrices to identity
    for (auto &bone_matrice : g_animation_bones[id].bone_matrices) { bone_matrice = MatrixIdentity(); }
    for (auto &prev_bone_matrice : g_animation_bones[id].prev_bone_matrices) { prev_bone_matrice = MatrixIdentity(); }

    // Auto-play first animation if model has animations
    if (g_world->animation[id].has_animations) {
        entity_set_animation(id, 0, true, 1.0F);
        entity_play_animation(id);
    }

    // If the model has no animations, make sure bone 0 stays identity
    // and vertices default to bone 0 with full weight.
    // (This ensures GPU skinning will have no visual effect.)
    if (!g_world->animation[id].has_animations) {
        g_world->animation[id].bone_count = 1;
    }

    return id;
}

void entity_destroy(EID id) {
    // The entity we destroy might be the one selected.
    if (g_world->selected_id == id) { g_world->selected_id = INVALID_EID; };

    // If this entity was an actor targeting something, remove it from target tracking
    if (g_world->type[id] == ENTITY_TYPE_NPC) { entity_actor_clear_actor_target(id); }

    g_world->flags[id] = 0;
    g_world->type[id]  = ENTITY_TYPE_NONE;

    // Clear any target tracking for this entity
    entity_actor_clear_target_tracker(id);

    g_world->active_ent_count--;
}

C8 const *entity_type_to_cstr(EntityType type) {
    return i_entity_type_strings[type];
}

Color entity_type_to_color(EntityType type) {
    return i_entity_type_color[type];
}

BOOL entity_is_valid(EID id) {
    return id != INVALID_EID &&
           id < WORLD_MAX_ENTITIES &&
           ENTITY_HAS_FLAG(g_world->flags[id], ENTITY_FLAG_IN_USE);
}

EID entity_find_at_mouse() {
    return entity_find_at_screen_point(input_get_mouse_position());
}

EID entity_find_at_mouse_with_type(EntityType type) {
    return entity_find_at_screen_point_with_type(input_get_mouse_position(), type);
}

EID entity_find_at_screen_point(Vector2 screen_point) {
    RayCollision collisions[WORLD_MAX_ENTITIES] = {};
    EID hits[WORLD_MAX_ENTITIES] = {};
    SZ hit_count = 0;

    Vector2 const res = render_get_render_resolution();
    Ray const ray     = GetScreenToWorldRayEx(screen_point, c3d_get(), (S32)res.x, (S32)res.y);

    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];

        RayCollision const collision = math_ray_collision_obb(ray, g_world->obb[i]);
        if (collision.hit) {
            collisions[hit_count] = collision;
            hits[hit_count]       = i;
            hit_count++;
        }
    }

    if (hit_count == 1) { return hits[0]; }

    if (hit_count > 1) {
        EID closest_id = hits[0];
        F32 closest_distance = collisions[0].distance;

        // We can skip the first one since we already set it as the closest.
        for (SZ i = 1; i < hit_count; ++i) {
            if (collisions[i].distance < closest_distance) {
                closest_distance = collisions[i].distance;
                closest_id       = hits[i];
            }
        }

        return closest_id;
    }

    return INVALID_EID;
}

EID entity_find_at_screen_point_with_type(Vector2 screen_point, EntityType type) {
    RayCollision collisions[WORLD_MAX_ENTITIES] = {};
    EID hits[WORLD_MAX_ENTITIES] = {};
    SZ hit_count = 0;

    Vector2 const res = render_get_render_resolution();
    Ray const ray     = GetScreenToWorldRayEx(screen_point, c3d_get(), (S32)res.x, (S32)res.y);

    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (g_world->type[i] != type) { continue; }

        RayCollision const collision = math_ray_collision_obb(ray, g_world->obb[i]);
        if (collision.hit) {
            collisions[hit_count] = collision;
            hits[hit_count]       = i;
            hit_count++;
        }
    }

    if (hit_count == 1) { return hits[0]; }

    if (hit_count > 1) {
        EID closest_id       = hits[0];
        F32 closest_distance = collisions[0].distance;

        // We can skip the first one since we already set it as the closest.
        for (SZ i = 1; i < hit_count; ++i) {
            if (collisions[i].distance < closest_distance) {
                closest_distance = collisions[i].distance;
                closest_id       = hits[i];
            }
        }

        return closest_id;
    }

    return INVALID_EID;
}

void entity_enable_talker(EID id, DialogueRoot *root, SZ nodes_count) {
    _assert_(g_world->type[id] == ENTITY_TYPE_NPC, "Only NPCs can have talkers.");

    EntityTalker *talker = &g_world->talker[id];
    talker->is_enabled                    = true;
    talker->conversation.root             = root;
    talker->conversation.nodes_count      = nodes_count;
    talker->conversation.current_node_ptr = find_dialogue_node_by_id(root, nodes_count, T_START_CSTR, false);

    _assert_(talker->conversation.current_node_ptr != nullptr, "While initializing the talker we could not find a T_START node");

    // We need to keep track that we actually somewhere do T_QUIT.
    BOOL is_quitting = false;

    // Check if referenced IDs for nodes and responses actually exist.
    for (SZ root_i = 0; root_i < nodes_count; ++root_i) {
        DialogueNode *node = &root[root_i];

        // If it has responses, we are dealing with a question.
        BOOL const is_question = node->response_count != 0;

        if (is_question) {
            llt("- Checking node [%zu]: '%s'", root_i, node->node_id->c);
            llt("--- Is a question");
            // If it is a question, we check if the referenced node_id_next in the responses exist in the nodes of the root.
            for (SZ resp_i = 0; resp_i < node->response_count; ++resp_i) {
                String *response_id      = node->responses[resp_i].response_id;
                String *response_id_next = node->responses[resp_i].node_id_next;

                llt("------ Response [%zu]: '%s' (next: '%s')", resp_i, response_id->c, response_id_next->c);

                if (string_equals_cstr(response_id_next, T_QUIT_CSTR)) {
                    is_quitting = true;
                    llt("------ Question ends with T_QUIT");
                    // Exception is the T_QUIT node, we accept that since we handle it as an exception.
                    continue;
                }

                BOOL found = false;

                for (SZ s_root_i = 0; s_root_i < nodes_count; ++s_root_i) {
                    DialogueNode *search_node = &root[s_root_i];

                    if (string_equals(response_id_next, search_node->node_id)) {
                        llt("-------- Found next node: '%s'", search_node->node_id->c);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    llt("-------- FAILED: Could not find: '%s'", response_id_next->c);
                    _assertf_(found, "Response '%s' references nonexistent node_id_next '%s'", response_id->c, response_id_next->c);
                }
            }
        } else {
            llt("- Checking node [%zu]: '%s' (next: '%s')", root_i, node->node_id->c, node->node_id_next->c);
            llt("--- Is a statement");

            String *node_id_next = node->node_id_next;

            // Okay, otherwise it is not a question but a statement. There is only one statement, unlike responses. So we can just iterate the roots
            // here again and linearly search for it. I just wanna love you, baby, yeah yeah yeah. But first, if we want T_QUIT we T_CONTINUE the fuck
            // outta here.
            if (string_equals_cstr(node_id_next, T_QUIT_CSTR)) {
                is_quitting = true;
                llt("------ Statement ends with T_QUIT");
                continue;
            }

            BOOL found = false;

            for (SZ s_root_i = 0; s_root_i < nodes_count; ++s_root_i) {
                DialogueNode *search_node = &root[s_root_i];

                if (string_equals(node_id_next, search_node->node_id)) {
                    llt("------ Found next node: '%s'", search_node->node_id->c);
                    found = true;
                    break;
                }
            }

            if (!found) {
                llt("------ FAILED: Could not find: '%s'", node_id_next->c);
                _assertf_(found, "Statement '%s' references nonexistent node_id_next '%s'", node->node_id->c, node_id_next->c);
            }
        }
    }

    if (!is_quitting) {
        llt("- FAILED: No T_QUIT found in whole dialogue.");
        _assert_(is_quitting, "No T_QUIT found in dialogue tree.");
    } else {
        llt("- PASSED");
    }
}

void entity_disable_talker(EID id) {
    g_world->talker[id].is_enabled = false;
}

void entity_enable_actor(EID id) {
    ENTITY_SET_FLAG(g_world->flags[id], ENTITY_FLAG_ACTOR);
}

void entity_disable_actor(EID id) {
    ENTITY_CLEAR_FLAG(g_world->flags[id], ENTITY_FLAG_ACTOR);
}

F32 entity_distance_to_position(EID id, Vector2 position) {
    Vector2 const pos = entity_get_position_vec2(id);
    OrientedBoundingBox const obb = g_world->obb[id];

    // Calculate distance to OBB edges
    Vector2 const offset = Vector2Subtract(position, pos);
    F32 const dx         = glm::max(0.0F, glm::abs(offset.x) - obb.extents.x);
    F32 const dz         = glm::max(0.0F, glm::abs(offset.y) - obb.extents.z);
    return math_sqrt_f32((dx * dx) + (dz * dz));
}

F32 entity_distance_to_entity(EID id, EID target_id) {
    Vector2 const pos                    = entity_get_position_vec2(id);
    Vector2 const target_pos             = entity_get_position_vec2(target_id);
    OrientedBoundingBox const obb        = g_world->obb[id];
    OrientedBoundingBox const target_obb = g_world->obb[target_id];

    // Distance between OBB centers
    Vector2 const offset = Vector2Subtract(target_pos, pos);

    // Calculate distance to each OBB edge
    F32 const dx = glm::max(0.0F, glm::abs(offset.x) - obb.extents.x - target_obb.extents.x);
    F32 const dz = glm::max(0.0F, glm::abs(offset.y) - obb.extents.z - target_obb.extents.z);
    return math_sqrt_f32((dx * dx) + (dz * dz));
}

Vector2 entity_get_approach_position_from(EID target_id, Vector2 from_position) {
    Vector2 const target_pos             = entity_get_position_vec2(target_id);
    OrientedBoundingBox const target_obb = g_world->obb[target_id];

    // Calculate direction and offset
    Vector2 const offset = Vector2Subtract(from_position, target_pos);

    // Clamp to OBB edges with buffer
    F32 const buffer = 0.5F;
    F32 const x_edge = (offset.x >= 0.0F) ? target_obb.extents.x + buffer : -(target_obb.extents.x + buffer);
    F32 const z_edge = (offset.y >= 0.0F) ? target_obb.extents.z + buffer : -(target_obb.extents.z + buffer);

    // Choose the edge that's closest to the direction
    Vector2 approach_offset;
    if (glm::abs(offset.x / target_obb.extents.x) > glm::abs(offset.y / target_obb.extents.z)) {
        // Approach from X side
        approach_offset = {x_edge, glm::clamp(offset.y, -target_obb.extents.z, target_obb.extents.z)};
    } else {
        // Approach from Z side
        approach_offset = {glm::clamp(offset.x, -target_obb.extents.x, target_obb.extents.x), z_edge};
    }

    return Vector2Add(target_pos, approach_offset);
}

BOOL entity_collides_sphere(EID id, Vector3 center, F32 radius) {
    return math_check_collision_obb_sphere(g_world->obb[id], center, radius);
}

BOOL entity_collides_sphere_outside(EID id, Vector3 center, F32 radius) {
    OrientedBoundingBox const obb = g_world->obb[id];
    return math_check_collision_obb_sphere(obb, center, radius) && !math_is_point_inside_obb(center, obb);
}

BOOL entity_collides_other(EID id, EID other_id) {
    OrientedBoundingBox const obb       = g_world->obb[id];
    OrientedBoundingBox const other_obb = g_world->obb[other_id];
    return math_check_collision_obbs(obb, other_obb);
}

BOOL entity_collides_player(EID id) {
    Vector3 const player_position = g_player.cameras[g_scenes.current_scene_type].position;
    F32 constexpr min_distance    = PLAYER_RADIUS * 10.0F;
    Vector3 const position        = g_world->position[id];
    OrientedBoundingBox const obb = g_world->obb[id];

    // If the distance to the entity is more than X units, we can skip the collision check.
    if (world_get_distance_to_player(position) > min_distance) { return false; }

    return math_check_collision_obb_sphere(obb, player_position, PLAYER_RADIUS);
}

F32 entity_distance_to_player(EID id) {
    return Vector3Distance(g_world->position[id], g_player.cameras[g_scenes.current_scene_type].position);
}

void entity_move(EID id, MoveDirection direction, F32 length) {
    F32 const rotation_rad = g_world->rotation[id] * DEG2RAD;
    Vector3 move_vector    = {0};

    switch (direction) {
        case MOVE_FORWARD: {
            move_vector = {math_sin_f32(rotation_rad), 0, math_cos_f32(rotation_rad)};
        } break;
        case MOVE_BACKWARD: {
            move_vector = {-math_sin_f32(rotation_rad), 0, -math_cos_f32(rotation_rad)};
        } break;
        case MOVE_LEFT: {
            move_vector = {-math_cos_f32(rotation_rad), 0, math_sin_f32(rotation_rad)};
        } break;
        case MOVE_RIGHT: {
            move_vector = {math_cos_f32(rotation_rad), 0, -math_sin_f32(rotation_rad)};
        } break;
        default:
            _unreachable_();
    }

    entity_add_position(id, Vector3Scale(move_vector, length));
    i_update_obb_extents_and_center(id);
}

void entity_add_position(EID id, Vector3 position) {
    g_world->position[id]   = Vector3Add(g_world->position[id], position);
    g_world->obb[id].center = Vector3Add(g_world->obb[id].center, position);
}

void entity_set_position(EID id, Vector3 position) {
    g_world->position[id] = position;
    i_update_obb_extents_and_center(id);
}

Vector2 entity_get_position_vec2(EID id) {
    Vector3 const posv3 = g_world->position[id];
    return {posv3.x, posv3.z};
}

void entity_set_rotation(EID id, F32 rotation) {
    // Normalize rotation
    rotation = math_mod_f32(rotation, 360.0F);
    if (rotation < 0.0F) { rotation += 360.0F; }

    g_world->rotation[id] = rotation;
    // Update OBB axes manually for non-physics entities
    F32 const cos_y = math_cos_f32(rotation * DEG2RAD);
    F32 const sin_y = math_sin_f32(rotation * DEG2RAD);
    g_world->obb[id].axes[0] = {cos_y, 0.0F, sin_y};
    g_world->obb[id].axes[1] = {0.0F, 1.0F, 0.0F};
    g_world->obb[id].axes[2] = {-sin_y, 0.0F, cos_y};

    i_update_obb_extents_and_center(id);
}

void entity_add_rotation(EID id, F32 rotation) {
    entity_set_rotation(id, g_world->rotation[id] + rotation);
}

void entity_face_target(EID id, EID target_id, F32 turn_speed, F32 dt) {
    Vector2 const pos           = entity_get_position_vec2(id);
    Vector2 const target_pos    = entity_get_position_vec2(target_id);
    Vector2 const dir_to_target = Vector2Normalize(Vector2Subtract(target_pos, pos));
    F32 const desired_angle     = (-math_atan2_f32(dir_to_target.y, dir_to_target.x) * RAD2DEG) + 90.0F;

    F32 current_rotation = g_world->rotation[id];
    F32 target_rotation  = desired_angle;

    while (current_rotation < 0.0F)    { current_rotation += 360.0F; }
    while (current_rotation >= 360.0F) { current_rotation -= 360.0F; }
    while (target_rotation < 0.0F)     { target_rotation += 360.0F; }
    while (target_rotation >= 360.0F)  { target_rotation -= 360.0F; }

    F32 delta_angle = target_rotation - current_rotation;
    while (delta_angle > 180.0F)  { delta_angle -= 360.0F; }
    while (delta_angle < -180.0F) { delta_angle += 360.0F; }

    F32 const max_turn_this_frame = turn_speed * dt;
    F32 turn_amount               = delta_angle;
    if (math_abs_f32(turn_amount) > max_turn_this_frame) { turn_amount = (turn_amount > 0.0F) ? max_turn_this_frame : -max_turn_this_frame; }

    entity_set_rotation(id, current_rotation + turn_amount);
}

void entity_add_scale(EID id, Vector3 scale) {
    g_world->scale[id] = Vector3Add(g_world->scale[id], scale);
    i_update_obb_extents_and_center(id);
}

void entity_set_scale(EID id, Vector3 scale) {
    g_world->scale[id] = scale;
    i_update_obb_extents_and_center(id);
}

void entity_set_model(EID id, C8 const *model_name) {
    // First we try to find the model in the asset manager
    AModel *model = asset_get_model(model_name);
    if (model == nullptr) {
        llw("could not set entity model, model with the name '%s' could not be found", model_name);
        return;
    }

    ou_strncpy(g_world->model_name[id], model_name, A_NAME_MAX_LENGTH);
    i_update_obb_extents_and_center(id);

    // Update animation flag and bone count when model changes
    g_world->animation[id].has_animations = model->has_animations;
    g_world->animation[id].bone_count     = model->base.boneCount;
    g_world->animation[id].anim_fps       = 60.0F;  // Default to 60 FPS as per raylib standard

    // Reset animation state when model changes
    g_world->animation[id].anim_index   = 0;
    g_world->animation[id].anim_frame   = 0;
    g_world->animation[id].anim_time    = 0.0F;
    g_world->animation[id].anim_playing = false;

    // Reset blending state
    g_world->animation[id].is_blending      = false;
    g_world->animation[id].blend_time       = 0.0F;
    g_world->animation[id].blend_duration   = 0.2F;
    g_world->animation[id].prev_anim_index  = 0;
    g_world->animation[id].prev_anim_frame  = 0;

    // Initialize bone matrices
    for (auto &bone_matrice : g_animation_bones[id].bone_matrices) { bone_matrice = MatrixIdentity(); }
    for (auto &prev_bone_matrice : g_animation_bones[id].prev_bone_matrices) { prev_bone_matrice = MatrixIdentity(); }

    // Auto-play first animation if new model has animations
    if (g_world->animation[id].has_animations) {
        entity_set_animation(id, 0, true, 1.0F);
        entity_play_animation(id);
    }
}

void entity_set_animation(EID id, U32 anim_index, BOOL loop, F32 speed) {
    AModel *model = asset_get_model(g_world->model_name[id]);
    if (!model || !model->has_animations)                { return; }
    if (anim_index >= (U32)model->animation_count)       { return; }
    if (anim_index == g_world->animation[id].anim_index) { return; }

    // Start blending from current animation to new animation
    g_world->animation[id].is_blending      = true;
    g_world->animation[id].blend_time       = 0.0F;
    g_world->animation[id].blend_duration   = 0.2F; // 200ms blend time
    g_world->animation[id].prev_anim_index  = g_world->animation[id].anim_index;
    g_world->animation[id].prev_anim_frame  = g_world->animation[id].anim_frame;

    // Store current bone matrices as previous state
    ou_memcpy(g_animation_bones[id].prev_bone_matrices,
              g_animation_bones[id].bone_matrices,
              sizeof(Matrix) * ENTITY_MAX_BONES);

    // Set new animation
    g_world->animation[id].anim_index = anim_index;
    g_world->animation[id].anim_loop  = loop;
    g_world->animation[id].anim_speed = speed;
    g_world->animation[id].anim_time  = 0.0F;
    g_world->animation[id].anim_frame = 0;
}

void entity_play_animation(EID id) {
    g_world->animation[id].anim_playing = true;
}

void entity_pause_animation(EID id) {
    g_world->animation[id].anim_playing = false;
}

void entity_stop_animation(EID id) {
    g_world->animation[id].anim_playing = false;
    g_world->animation[id].anim_time    = 0.0F;
    g_world->animation[id].anim_frame   = 0;
}

void entity_set_animation_frame(EID id, U32 frame) {
    AModel *model = asset_get_model(g_world->model_name[id]);
    if (!model || !model->has_animations)                                 { return; }
    if (g_world->animation[id].anim_index >= (U32)model->animation_count) { return; }

    ModelAnimation const& anim = model->animations[g_world->animation[id].anim_index];
    if (frame >= (U32)anim.frameCount) { return; }

    g_world->animation[id].anim_frame = frame;
    // Update time to match frame using entity's FPS
    g_world->animation[id].anim_time = (F32)frame / g_world->animation[id].anim_fps;
}

BOOL entity_damage(EID id, S32 damage) {
    EntityHealth *health = &g_world->health[id];

    health->current -= damage;

    if (g_world->type[id] == ENTITY_TYPE_NPC) {
        audio_set_pitch(ACG_VOICE, 1.5F);
        audio_play_3d_at_position(ACG_VOICE, TS("hurt_%d.ogg", random_s32(0, 2))->c, g_world->position[id]);
    }

    Vector3 const hit_pos = g_world->position[id];

    if (health->current <= 0) {
        if (g_world->type[id] == ENTITY_TYPE_NPC) {
            if (random_s32(0, 9) == 0) {
                audio_set_pitch(ACG_VOICE, 1.5F);
                audio_play_3d_at_position(ACG_VOICE, "death_0.ogg", g_world->position[id]);
            }
        }
        audio_play_3d_at_position(ACG_SFX, "plop.ogg", g_world->position[id]);

        // Spawn a headstone where the entity was
        Vector3 const death_pos = g_world->position[id];
        Color const entity_color = g_world->tint[id];

        entity_create(ENTITY_TYPE_PROP,
                      TS("Headstone for %s", g_world->name[id])->c,
                      death_pos,
                      g_world->rotation[id],
                      g_world->scale[id] / 2.0F,
                      entity_color,
                      "headstone.glb");

        entity_destroy(id);

        // Big blood explosion on death
        particles3d_add_effect_blood_death(hit_pos, RED, MAROON, 0.3F, 150);

        return true;
    }

    // Smaller blood spray on non-lethal hit
    particles3d_add_effect_blood_hit(hit_pos, RED, PINK, 0.5F, 100);

    return false;
}

// TODO: For now we are assuming that the name is unique.
// TODO: This is a linear search, we can do better.
EID entity_find_by_name(C8 const *name) {
    EID found = INVALID_EID;

    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];

        if (ou_strcmp(g_world->name[i], name) == 0) {
            if (found != INVALID_EID) { llw("We found another entity (%u) with the name %s", i, name); }
            found = i;
        }
    }

    return found;
}
