#include "math.hpp"

#include "asset.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "log.hpp"
#include "map.hpp"
#include "memory.hpp"
#include "message.hpp"
#include "player.hpp"
#include "std.hpp"
#include "world.hpp"

#include <raymath.h>

// Text Cache
struct ITextCacheKey {
    U32 name_hash;
    S32 font_size;
    U32 text_hash;
};
struct ITextCacheValue {
    Vector2 size;
};
U64 static inline i_text_cache_hash(ITextCacheKey key) {
    U64 hash = hash_u64((U64)key.name_hash);
    hash    ^= hash_u64((U64)key.font_size);
    hash    ^= hash_u64((U64)key.text_hash);
    return hash;
}
BOOL static inline i_text_cache_equal(ITextCacheKey a, ITextCacheKey b) {
    return a.name_hash == b.name_hash && a.font_size == b.font_size && a.text_hash == b.text_hash;
}
#define TEXT_CACHE_INITIAL_CAPACITY 1024
MAP_DECLARE(ITextCache, ITextCacheKey, ITextCacheValue, i_text_cache_hash, i_text_cache_equal)

struct IMathCache {
    ITextCache text;
};

IMathCache static i_cache = {};

void math_init() {
    random_seed(RANDOM_SEED);
    ITextCache_init(&i_cache.text, MEMORY_TYPE_ARENA_MATH, TEXT_CACHE_INITIAL_CAPACITY);
}

void math_update() {
    ArenaStats const stats = memory_get_current_arena_stats(MEMORY_TYPE_ARENA_MATH);
    F32 const usage_percent = (F32)stats.total_used / (F32)stats.total_capacity;

    if (usage_percent > 0.95F) {
        memory_reset_type(MEMORY_TYPE_ARENA_MATH);
        ITextCache_init(&i_cache.text, MEMORY_TYPE_ARENA_MATH, TEXT_CACHE_INITIAL_CAPACITY);
    }
}

Vector2 measure_text(AFont *font, C8 const *text) {
    U32 const name_hash     = (U32)hash_cstr(font->header.name);
    U32 const text_hash     = (U32)hash_cstr(text);
    ITextCacheKey const key = {name_hash, font->font_size, text_hash};

    ITextCacheValue *cached = ITextCache_get(&i_cache.text, key);
    if (cached != nullptr) { return cached->size; }

    Vector2 const size          = MeasureTextEx(font->base, text, (F32)font->font_size, 0.0F);
    ITextCacheValue const value = {size};
    ITextCache_insert(&i_cache.text, key, value);

    return size;
}

Vector2 measure_text_ouc(AFont *font, C8 const *text) {
    U32 const name_hash     = (U32)hash_cstr(font->header.name);
    U32 const text_hash     = (U32)hash_cstr(text);
    ITextCacheKey const key = {name_hash, font->font_size, text_hash};

    ITextCacheValue *cached = ITextCache_get(&i_cache.text, key);
    if (cached != nullptr) { return cached->size; }

    C8 clean_text[OUC_MAX_SEGMENT_LENGTH] = {};
    SZ clean_idx = 0;

    SZ i = 0;
    while (i < ou_strlen(text)) {
        if (text[i] == '\\' && ou_strncmp(&text[i], "\\ouc{", 5) == 0) {
            // Find the end of the escape code
            C8 const *end_escape = ou_strchr(&text[i], '}');
            if (end_escape == nullptr) {
                break;  // No matching '}', exit loop
            }

            // Move idx past the escape code
            i = (SZ)(end_escape - text + 1);
        } else {
            if (clean_idx < OUC_MAX_SEGMENT_LENGTH - 1) {
                clean_text[clean_idx++] = text[i++];
            } else {
                llw("Clean text exceeds static buffer size.\n");
                break;
            }
        }
    }
    clean_text[clean_idx] = '\0';

    Vector2 const size          = MeasureTextEx(font->base, clean_text, (F32)font->font_size, 0.0F);
    ITextCacheValue const value = {size};
    ITextCache_insert(&i_cache.text, key, value);

    return size;
}

SZ math_levenshtein_distance(C8 const *a, C8 const *b) {
    SZ const len_a = ou_strlen(a);
    SZ const len_b = ou_strlen(b);
    auto *matrix   = mmta(SZ *, sizeof(SZ) * (len_a + 1) * (len_b + 1));

    for (SZ i = 0; i <= len_a; i++) { matrix[i * (len_b + 1)] = i; }
    for (SZ i = 0; i <= len_b; i++) { matrix[i] = i; }
    for (SZ i = 1; i <= len_a; i++) {
        for (SZ j = 1; j <= len_b; j++) {
            SZ const cost = a[i - 1] == b[j - 1] ? 0 : 1;
            SZ const deletion = matrix[((i - 1) * (len_b + 1)) + j] + 1;
            SZ const insertion = matrix[(i * (len_b + 1)) + j - 1] + 1;
            SZ const substitution = matrix[((i - 1) * (len_b + 1)) + j - 1] + cost;
            matrix[(i * (len_b + 1)) + j] = glm::min(deletion, glm::min(insertion, substitution));
        }
    }

    SZ const result = matrix[(len_a * (len_b + 1)) + len_b];

    return result;
}

Vector3 math_find_lowest_point_in_model(Model *model, Matrix transform) {
    Vector3 lowest = {F32_MAX, F32_MAX, F32_MAX};

    // Iterate over all meshes in the model.
    for (SZ i = 0; i < (SZ)model->meshCount; ++i) {
        Mesh *mesh = &model->meshes[i];

        // Iterate over all vertices in the mesh.
        // The vertices are stored in a single array of floats, 3 components (XYZ) per vertex.
        for (SZ j = 0; j < (SZ)mesh->vertexCount; ++j) {
            Vector3 vertex = {
                mesh->vertices[3 * j],
                mesh->vertices[(3 * j) + 1],
                mesh->vertices[(3 * j) + 2],
            };

            // Transform the vertex by the model matrix.
            vertex = Vector3Transform(vertex, transform);

            // Check if the vertex is lower than the current lowest point.
            if (vertex.y < lowest.y) { lowest = vertex; }
        }
    }

    return lowest;
}

Vector3 math_find_highest_point_in_model(Model *model, Matrix transform) {
    Vector3 highest = {F32_MIN, F32_MIN, F32_MIN};

    // Iterate over all meshes in the model.
    for (SZ i = 0; i < (SZ)model->meshCount; ++i) {
        Mesh *mesh = &model->meshes[i];

        // Iterate over all vertices in the mesh.
        // The vertices are stored in a single array of floats, 3 components (XYZ) per vertex.
        for (SZ j = 0; j < (SZ)mesh->vertexCount; ++j) {
            Vector3 vertex = {
                mesh->vertices[3 * j],
                mesh->vertices[(3 * j) + 1],
                mesh->vertices[(3 * j) + 2],
            };

            // Transform the vertex by the model matrix.
            vertex = Vector3Transform(vertex, transform);

            // Check if the vertex is higher than the current highest point.
            if (vertex.y > highest.y) { highest = vertex; }
        }
    }

    return highest;
}

BOOL math_is_point_inside_bb(Vector3 point, BoundingBox box) {
    return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y && point.y <= box.max.y && point.z >= box.min.z && point.z <= box.max.z;
}

BOOL math_is_point_inside_obb(Vector3 point, OrientedBoundingBox box) {
    // Transform point to box's local space
    Vector3 const local_point = Vector3Subtract(point, box.center);

    // Project point onto box's local axes
    Vector3 proj;
    proj.x = Vector3DotProduct(local_point, box.axes[0]);
    proj.y = Vector3DotProduct(local_point, box.axes[1]);
    proj.z = Vector3DotProduct(local_point, box.axes[2]);

    // Check if projected point is within box extents
    return math_abs_f32(proj.x) <= box.extents.x && math_abs_f32(proj.y) <= box.extents.y && math_abs_f32(proj.z) <= box.extents.z;
}

RayCollision math_ray_collision_obb(Ray ray, OrientedBoundingBox box) {
    RayCollision collision = {};

    // Transform ray into box's local space
    Vector3 local_ray_pos;
    Vector3 local_ray_dir;

    // Position transformation
    Vector3 const rel_pos = Vector3Subtract(ray.position, box.center);

    local_ray_pos.x = Vector3DotProduct(rel_pos, box.axes[0]);
    local_ray_pos.y = Vector3DotProduct(rel_pos, box.axes[1]);
    local_ray_pos.z = Vector3DotProduct(rel_pos, box.axes[2]);

    // Direction transformation
    local_ray_dir.x = Vector3DotProduct(ray.direction, box.axes[0]);
    local_ray_dir.y = Vector3DotProduct(ray.direction, box.axes[1]);
    local_ray_dir.z = Vector3DotProduct(ray.direction, box.axes[2]);

    // Now we can treat this as an AABB intersection in local space
    // Check if ray origin is inside box
    BOOL const inside_box =
        (math_abs_f32(local_ray_pos.x) <= box.extents.x) &&
        (math_abs_f32(local_ray_pos.y) <= box.extents.y) &&
        (math_abs_f32(local_ray_pos.z) <= box.extents.z);

    if (inside_box) { local_ray_dir = Vector3Negate(local_ray_dir); }

    // Compute intersection with all six box faces
    F32 t[11]          = {0};
    t[8]               = !F32_EQUAL(local_ray_dir.x, 0.0F) ? 1.0F / local_ray_dir.x : 0.0F;
    t[9]               = !F32_EQUAL(local_ray_dir.y, 0.0F) ? 1.0F / local_ray_dir.y : 0.0F;
    t[10]              = !F32_EQUAL(local_ray_dir.z, 0.0F) ? 1.0F / local_ray_dir.z : 0.0F;
    t[0]               = (-box.extents.x - local_ray_pos.x) * t[8];
    t[1]               = (box.extents.x - local_ray_pos.x) * t[8];
    t[2]               = (-box.extents.y - local_ray_pos.y) * t[9];
    t[3]               = (box.extents.y - local_ray_pos.y) * t[9];
    t[4]               = (-box.extents.z - local_ray_pos.z) * t[10];
    t[5]               = (box.extents.z - local_ray_pos.z) * t[10];
    t[6]               = glm::max(glm::max(glm::min(t[0], t[1]), glm::min(t[2], t[3])), glm::min(t[4], t[5]));
    t[7]               = glm::min(glm::min(glm::max(t[0], t[1]), glm::max(t[2], t[3])), glm::max(t[4], t[5]));
    collision.hit      = (t[7] >= 0) && (t[6] <= t[7]);
    collision.distance = t[6];

    // Transform intersection point back to world space
    if (collision.hit) {
        Vector3 const local_hit = {
            local_ray_pos.x + (local_ray_dir.x * collision.distance),
            local_ray_pos.y + (local_ray_dir.y * collision.distance),
            local_ray_pos.z + (local_ray_dir.z * collision.distance),
        };

        // Transform point back to world space
        collision.point = box.center;
        collision.point = Vector3Add(collision.point, Vector3Add(Vector3Scale(box.axes[0], local_hit.x), Vector3Add(Vector3Scale(box.axes[1], local_hit.y), Vector3Scale(box.axes[2], local_hit.z))));

        // Calculate normal in local space
        Vector3 local_normal = {0};
        // Use the intersection point in local space to determine which face was hit
        if (F32_EQUAL(math_abs_f32(local_hit.x), box.extents.x)) {
            local_normal.x = local_hit.x > 0 ? 1.0F : -1.0F;
        } else if (F32_EQUAL(math_abs_f32(local_hit.y), box.extents.y)) {
            local_normal.y = local_hit.y > 0 ? 1.0F : -1.0F;
        } else if (F32_EQUAL(math_abs_f32(local_hit.z), box.extents.z)) {
            local_normal.z = local_hit.z > 0 ? 1.0F : -1.0F;
        }

        // Transform normal back to world space
        collision.normal = Vector3Add(Vector3Add(Vector3Scale(box.axes[0], local_normal.x), Vector3Scale(box.axes[1], local_normal.y)), Vector3Scale(box.axes[2], local_normal.z));
        collision.normal = Vector3Normalize(collision.normal);

        if (inside_box) {
            // Reset ray.direction
            local_ray_dir       = Vector3Negate(local_ray_dir);
            collision.distance *= -1.0F;
            collision.normal    = Vector3Negate(collision.normal);
        }
    }

    return collision;
}

BOOL math_check_collision_obb_sphere(OrientedBoundingBox box, Vector3 center, F32 radius) {
    // Transform sphere center into box's local space
    Vector3 const local_center = Vector3Subtract(center, box.center);
    Vector3 const point = {
        Vector3DotProduct(local_center, box.axes[0]),
        Vector3DotProduct(local_center, box.axes[1]),
        Vector3DotProduct(local_center, box.axes[2]),
    };

    // Quick AABB test
    if (math_abs_f32(point.x) > (box.extents.x + radius) || math_abs_f32(point.y) > (box.extents.y + radius) ||
        math_abs_f32(point.z) > (box.extents.z + radius)) {
        return false;
    }

    // Find closest point in box to sphere center (in local space)
    Vector3 const closest = {
        glm::clamp(point.x, -box.extents.x, box.extents.x),
        glm::clamp(point.y, -box.extents.y, box.extents.y),
        glm::clamp(point.z, -box.extents.z, box.extents.z),
    };

    // Inside check
    if (F32_EQUAL(point.x, closest.x) && F32_EQUAL(point.y, closest.y) && F32_EQUAL(point.z, closest.z)) { return true; }

    // Transform closest point back to world space
    Vector3 const world_closest = Vector3Add(box.center, Vector3Add(Vector3Add(Vector3Scale(box.axes[0], closest.x), Vector3Scale(box.axes[1], closest.y)), Vector3Scale(box.axes[2], closest.z)));

    return Vector3LengthSqr(Vector3Subtract(world_closest, center)) <= (radius * radius);
}

BOOL math_check_collision_obbs(OrientedBoundingBox box1, OrientedBoundingBox box2) {
    // Using Separating Axis Theorem (SAT)
    Vector3 axes[15];  // 15 potential separating axes to test

    // Get the 6 face normals (3 from each box)
    for (S32 i = 0; i < 3; i++) {
        axes[i]     = box1.axes[i];
        axes[i + 3] = box2.axes[i];
    }

    // Get the 9 edge cross products
    S32 idx = 6;
    for (auto _axe : box1.axes) {
        for (auto j : box2.axes) {
            axes[idx] = Vector3CrossProduct(_axe, j);
            // Skip if cross product is zero (parallel edges)
            if (Vector3Length(axes[idx]) > 0.001F) {
                axes[idx] = Vector3Normalize(axes[idx]);
                idx++;
            }
        }
    }

    // Test all axes
    Vector3 const translation = Vector3Subtract(box2.center, box1.center);

    for (S32 i = 0; i < idx; i++) {
        F32 const proj_1 = math_abs_f32(Vector3DotProduct(Vector3Scale(box1.axes[0], box1.extents.x), axes[i])) +
                           math_abs_f32(Vector3DotProduct(Vector3Scale(box1.axes[1], box1.extents.y), axes[i])) +
                           math_abs_f32(Vector3DotProduct(Vector3Scale(box1.axes[2], box1.extents.z), axes[i]));

        F32 const proj_2 = math_abs_f32(Vector3DotProduct(Vector3Scale(box2.axes[0], box2.extents.x), axes[i])) +
                           math_abs_f32(Vector3DotProduct(Vector3Scale(box2.axes[1], box2.extents.y), axes[i])) +
                           math_abs_f32(Vector3DotProduct(Vector3Scale(box2.axes[2], box2.extents.z), axes[i]));

        F32 const translation_proj = math_abs_f32(Vector3DotProduct(translation, axes[i]));

        // If we find a separating axis, the boxes don't intersect
        if (translation_proj > proj_1 + proj_2) { return false; }
    }

    // No separating axis found, the boxes must intersect
    return true;
}

Vector3 math_obb_get_bottom_center(OrientedBoundingBox obb) {
    Vector3 const down_offset = Vector3Scale(obb.axes[1], -obb.extents.y);
    return Vector3Add(obb.center, down_offset);
}

Vector3 math_obb_get_top_center(OrientedBoundingBox obb) {
    Vector3 const up_offset = Vector3Scale(obb.axes[1], obb.extents.y);
    return Vector3Add(obb.center, up_offset);
}

F32 math_obb_get_height(OrientedBoundingBox obb) {
    return obb.extents.y * 2.0F;
}

F32 math_obb_get_bottom_y(OrientedBoundingBox obb) {
    Vector3 const bottom_center = math_obb_get_bottom_center(obb);
    return bottom_center.y;
}

F32 math_obb_get_top_y(OrientedBoundingBox obb) {
    Vector3 const top_center = math_obb_get_top_center(obb);
    return top_center.y;
}

void math_fill_transform_matrix(Matrix *dst, Vector3 position, Vector3 rotation, Vector3 scale) {
    // Create a translation matrix
    Matrix const translation = MatrixTranslate(position.x, position.y, position.z);

    // Create rotation matrices - negate the angles to match OBB rotation
    Matrix const rotationX = MatrixRotateX(-rotation.x * DEG2RAD);
    Matrix const rotationY = MatrixRotateY(-rotation.y * DEG2RAD);
    Matrix const rotationZ = MatrixRotateZ(-rotation.z * DEG2RAD);

    // Combine the rotation matrices (order matters: Z * Y * X)
    Matrix const rotationMatrix = MatrixMultiply(MatrixMultiply(rotationZ, rotationY), rotationX);

    // Create a scale matrix
    Matrix const scaleMatrix = MatrixScale(scale.x, scale.y, scale.z);

    // Combine translation, rotation, and scale into a final transformation matrix
    *dst = MatrixMultiply(MatrixMultiply(scaleMatrix, rotationMatrix), translation);
}

RayCollision math_ray_collision_to_terrain(ATerrain *terrain, Vector3 position, Vector3 direction) {
    return GetRayCollisionMesh({position, direction}, terrain->mesh, terrain->transform);
}

Vector3 math_keep_entity_on_ground(ATerrain *terrain, Vector3 position, F32 dt, F32 *fall_velocity, F32 *last_height, F32 *ground_pos, F32 height_offset) {
    // Reset fall velocity if moving upward
    if (position.y > *last_height) { *fall_velocity = 0.0F; }
    *last_height = position.y;

    // Check ground collision
    F32 const ground_y = math_get_terrain_height(terrain, position.x, position.z);
    if (ground_y < position.y) {  // The player is above the ground
        *ground_pos = ground_y;
    } else {  // The player is below the ground
        position.y = ground_y + height_offset;
    }

    F32 const target_height = *ground_pos + height_offset;

    // Apply gravity
    if (position.y > target_height) {
        *fall_velocity += GRAVITY * dt;
        position.y     -= *fall_velocity * dt;
    } else {
        *fall_velocity = 0.0F;
    }

    // Keep above ground, otherwise the player will basically jump up and down.
    position.y = glm::max(position.y, target_height);

    return position;
}

void math_keep_player_on_ground(ATerrain *terrain, F32 dt, Player *player) {
    if (c_debug__noclip) { return; }

    Camera3D *player_camera = &player->camera3d;

    Vector3 const new_pos =
        math_keep_entity_on_ground(terrain,
                                   player_camera->position,
                                   dt,
                                   &player->keep_on_ground_state.fall_velocity,
                                   &player->keep_on_ground_state.last_height,
                                   &player->keep_on_ground_state.ground_pos,
                                   PLAYER_HEIGHT_TO_EYES);

    F32 const height_diff    = new_pos.y - player_camera->position.y;
    player_camera->position  = new_pos;
    player_camera->target.y += height_diff;

    // Force above ground (might happen when we are fast-forwarding)
    F32 const terrain_y = math_get_terrain_height(terrain, player_camera->position.x, player_camera->position.z);
    if (player_camera->position.y < terrain_y) {
        F32 const force_height_diff = (terrain_y + PLAYER_HEIGHT_TO_EYES) - player_camera->position.y;
        player_camera->position.y = terrain_y + PLAYER_HEIGHT_TO_EYES;
        player_camera->target.y  += force_height_diff;
    }
}

F32 math_get_terrain_height(ATerrain const *terrain, F32 world_x, F32 world_z) {
    // Normalize coordinates relative to terrain dimensions
    F32 normalized_x = world_x / terrain->dimensions.x;
    F32 normalized_z = world_z / terrain->dimensions.z;

    // Clamp to valid range
    normalized_x = glm::max(0.0F, glm::min(1.0F, normalized_x));
    normalized_z = glm::max(0.0F, glm::min(1.0F, normalized_z));

    // Convert to grid coordinates
    F32 const grid_x = normalized_x * (F32)(terrain->height_field_width - 1);
    F32 const grid_z = normalized_z * (F32)(terrain->height_field_height - 1);

    // Get grid cell coordinates
    U32 const x0 = (U32)glm::floor(grid_x);
    U32 const z0 = (U32)glm::floor(grid_z);
    U32 const x1 = glm::min(x0 + 1, terrain->height_field_width - 1);
    U32 const z1 = glm::min(z0 + 1, terrain->height_field_height - 1);

    // Get stored heights
    F32 const h00 = terrain->height_field[(z0 * terrain->height_field_width) + x0];
    F32 const h10 = terrain->height_field[(z0 * terrain->height_field_width) + x1];
    F32 const h01 = terrain->height_field[(z1 * terrain->height_field_width) + x0];
    F32 const h11 = terrain->height_field[(z1 * terrain->height_field_width) + x1];

    // Interpolation factors
    F32 const fx = grid_x - (F32)x0;
    F32 const fz = grid_z - (F32)z0;

    // Bilinear interpolation
    F32 const h0 = (h00 * (1.0F - fx)) + (h10 * fx);
    F32 const h1 = (h01 * (1.0F - fx)) + (h11 * fx);

    return (h0 * (1.0F - fz)) + (h1 * fz);
}

Vector3 math_get_terrain_normal(ATerrain const *terrain, F32 world_x, F32 world_z) {
    // Sample points for calculating normal
    F32 const sample_distance = 1.0F;

    // Get heights at neighboring points
    F32 const height_center  = math_get_terrain_height(terrain, world_x, world_z);
    F32 const height_right   = math_get_terrain_height(terrain, world_x + sample_distance, world_z);
    F32 const height_forward = math_get_terrain_height(terrain, world_x, world_z + sample_distance);

    // Create vectors along the terrain surface
    Vector3 const right_vector   = {sample_distance, height_right - height_center, 0.0F};
    Vector3 const forward_vector = {0.0F, height_forward - height_center, sample_distance};

    // Cross product to get normal vector
    Vector3 const normal = Vector3CrossProduct(forward_vector, right_vector);

    // Normalize the result
    return Vector3Normalize(normal);
}

F32 math_calculate_y_rotation(F32 pos_x, F32 pos_z, F32 tgt_x, F32 tgt_z) {
    F32 const dir_x = tgt_x - pos_x;
    F32 const dir_z = tgt_z - pos_z;
    return math_atan2_f32(dir_x, dir_z) * RAD2DEG;
}

F32 math_normalize_angle(F32 angle) {
    while (angle > 360.0F) { angle -= 360.0F; }
    while (angle < 0.0F) { angle += 360.0F; }
    return angle;
}

F32 math_shortest_angle_distance(F32 start, F32 end) {
    start = math_normalize_angle(start);
    end = math_normalize_angle(end);
    F32 diff = end - start;
    if (diff > 180.0F) {
        diff -= 360.0F;
    } else if (diff < -180.0F) {
        diff += 360.0F;
    }
    return diff;
}

F32 math_normalize_angle_0_360(F32 angle) {
    while (angle < 0.0F) { angle += 360.0F; }
    while (angle >= 360.0F) { angle -= 360.0F; }
    return angle;
}

F32 math_normalize_angle_delta(F32 delta) {
    while (delta > 180.0F) { delta -= 360.0F; }
    while (delta < -180.0F) { delta += 360.0F; }
    return delta;
}

// Möller-Trumbore ray-triangle intersection algorithm
// Returns true if ray intersects triangle, with distance in out_t
// This is a production-quality, well-tested algorithm used industry-wide
BOOL math_ray_triangle_intersection(Vector3 ray_origin, Vector3 ray_direction, Vector3 v0, Vector3 v1, Vector3 v2, F32 *out_t) {
    F32 constexpr epsilon = 0.0000001F;

    Vector3 const edge1 = Vector3Subtract(v1, v0);
    Vector3 const edge2 = Vector3Subtract(v2, v0);
    Vector3 const h = Vector3CrossProduct(ray_direction, edge2);
    F32 const a = Vector3DotProduct(edge1, h);

    // Ray is parallel to triangle
    if (a > -epsilon && a < epsilon) {
        return false;
    }

    F32 const f = 1.0F / a;
    Vector3 const s = Vector3Subtract(ray_origin, v0);
    F32 const u = f * Vector3DotProduct(s, h);

    // Intersection is outside triangle
    if (u < 0.0F || u > 1.0F) {
        return false;
    }

    Vector3 const q = Vector3CrossProduct(s, edge1);
    F32 const v = f * Vector3DotProduct(ray_direction, q);

    // Intersection is outside triangle
    if (v < 0.0F || u + v > 1.0F) {
        return false;
    }

    // Compute intersection distance
    F32 const t = f * Vector3DotProduct(edge2, q);

    // Ray intersection (t > epsilon ensures we don't hit backfaces or very close triangles)
    if (t > epsilon) {
        if (out_t) {
            *out_t = t;
        }
        return true;
    }

    return false;
}

// AABB-sphere intersection test
// Returns true if sphere intersects or is contained in AABB
BOOL math_aabb_sphere_intersection(Vector3 aabb_min, Vector3 aabb_max, Vector3 sphere_center, F32 sphere_radius) {
    // Find closest point on AABB to sphere center
    F32 const closest_x = glm::clamp(sphere_center.x, aabb_min.x, aabb_max.x);
    F32 const closest_y = glm::clamp(sphere_center.y, aabb_min.y, aabb_max.y);
    F32 const closest_z = glm::clamp(sphere_center.z, aabb_min.z, aabb_max.z);

    // Calculate distance from sphere center to closest point
    F32 const dx = sphere_center.x - closest_x;
    F32 const dy = sphere_center.y - closest_y;
    F32 const dz = sphere_center.z - closest_z;
    F32 const distance_sq = (dx * dx) + (dy * dy) + (dz * dz);

    return distance_sq <= (sphere_radius * sphere_radius);
}

// Transform AABB by position and scale (assumes no rotation)
BoundingBox math_transform_aabb(BoundingBox bb, Vector3 position, Vector3 scale) {
    Vector3 const center = {
        (bb.min.x + bb.max.x) * 0.5F,
        (bb.min.y + bb.max.y) * 0.5F,
        (bb.min.z + bb.max.z) * 0.5F
    };

    Vector3 const extents = {
        (bb.max.x - bb.min.x) * 0.5F,
        (bb.max.y - bb.min.y) * 0.5F,
        (bb.max.z - bb.min.z) * 0.5F
    };

    Vector3 const scaled_center = {
        center.x * scale.x,
        center.y * scale.y,
        center.z * scale.z
    };

    Vector3 const scaled_extents = {
        extents.x * scale.x,
        extents.y * scale.y,
        extents.z * scale.z
    };

    Vector3 const world_center = Vector3Add(scaled_center, position);

    BoundingBox result;
    result.min = Vector3Subtract(world_center, scaled_extents);
    result.max = Vector3Add(world_center, scaled_extents);

    return result;
}
