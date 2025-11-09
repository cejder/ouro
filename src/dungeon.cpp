#include "dungeon.hpp"
#include <stdio.h>
#include "array.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "common.hpp"
#include "cvar.hpp"
#include "memory.hpp"
#include "raymath.h"
#include "render.hpp"
#include "player.hpp"

#include <raylib.h>

#define DUNGEON_TILE_SIZE 10.0F
#define DUNGEON_WALL_HEIGHT_MULTIPLIER 2.5F
#define DUNGEON_CEILING_THICKNESS 10.0F
#define DUNGEON_FLOOR_THICKNESS 0.1F
#define FOOTSTEP_DISTANCE_THRESHOLD 4.0F
#define WALL_BUFFER_DISTANCE 2.0F

enum DungeonTileType : U8 {
    DUNGEON_TILE_TYPE_NONE,
    DUNGEON_TILE_TYPE_BASIC_FLOOR,
    DUNGEON_TILE_TYPE_BASIC_WALL,
    DUNGEON_TILE_TYPE_PLAYER_POSITION,
};

struct IDungeonCollisionWall {
    Vector3 position;
    Vector3 size;
};

// NOTE: Start X at -1 so first tile starts at x=0 after increment
#define DUNGEON_START_X -1
#define DUNGEON_START_Y 0

DungeonTileType static i_parse_next_type(C8 const *dun_text, S32 *x, S32 *y) {
    switch (*dun_text) {
        case '.': {
            *x += 1;
            return DUNGEON_TILE_TYPE_BASIC_FLOOR;
        };

        case 'w': {
            *x += 1;
          return DUNGEON_TILE_TYPE_BASIC_WALL;
        };

        case '\n': {
          *x = DUNGEON_START_X;
          *y += 1;
          return DUNGEON_TILE_TYPE_NONE;
        };

        case ' ': {
          *x += 1;
          return DUNGEON_TILE_TYPE_NONE;
        };

        case 'p': {
          *x += 1;
          return DUNGEON_TILE_TYPE_PLAYER_POSITION;
        }

        default: {
          _unreachable_();
          return DUNGEON_TILE_TYPE_NONE;
        }
    }
}

struct IDungeonTileData {
    DungeonTileType type;
    Vector3 position;
    Vector3 size;
    Color tint;
};

ARRAY_DECLARE(DungeonTileArray, IDungeonTileData);
ARRAY_DECLARE(DungeonCollisionWallArray, IDungeonCollisionWall);

struct IDungeonData {
    Vector2 player_position;
    DungeonTileArray tiles;
    DungeonCollisionWallArray collision_walls;
    Model combined_model;
};

IDungeonData static i_parse_dungeon(C8 const* filepath) {
    IDungeonData data = {};
    array_init(MEMORY_TYPE_ARENA_PERMANENT, &data.tiles, (SZ)(100 * 100));
    array_init(MEMORY_TYPE_ARENA_PERMANENT, &data.collision_walls, (SZ)(100 * 100));

    IDungeonTileData tdata = {};

    C8 *ptr =  LoadFileText(filepath);

    F32 const tile_size       = DUNGEON_TILE_SIZE;
    Vector3 const wall_dim    = {tile_size, tile_size * DUNGEON_WALL_HEIGHT_MULTIPLIER, tile_size};
    Vector3 const floor_dim   = {tile_size, DUNGEON_FLOOR_THICKNESS, tile_size};
    Vector3 const ceiling_dim = {tile_size, DUNGEON_CEILING_THICKNESS, tile_size};
    S32 curr_x                = DUNGEON_START_X;
    S32 curr_y                = DUNGEON_START_Y;

    while (*ptr != '\0') {
        DungeonTileType const type = i_parse_next_type(ptr, &curr_x, &curr_y);
        switch (type) {
            case DUNGEON_TILE_TYPE_NONE: {
                // Do nothing.
            } break;

            case DUNGEON_TILE_TYPE_BASIC_FLOOR: {
              // Floor
              tdata.type     = type;
              tdata.position = {(F32)curr_x * tile_size, 0.0F, (F32)curr_y * tile_size};
              tdata.size     = floor_dim;
              tdata.tint     = DARKBROWN;
              array_push(&data.tiles, tdata);

              // Ceiling
              tdata.type     = type;
              tdata.position = {(F32)curr_x * tile_size, wall_dim.y - ceiling_dim.y, (F32)curr_y * tile_size};
              tdata.size     = ceiling_dim;
              tdata.tint     = BEIGE;
              array_push(&data.tiles, tdata);
            } break;

            case DUNGEON_TILE_TYPE_BASIC_WALL: {
              tdata.type     = type;
              tdata.position = {(F32)curr_x * tile_size, 0.0F, (F32)curr_y * tile_size};
              tdata.size     = wall_dim;
              tdata.tint     = BEIGE;
              array_push(&data.tiles, tdata);

              // Add collision data for this wall
              IDungeonCollisionWall wall = {};
              wall.position              = tdata.position;
              wall.size                  = wall_dim;
              array_push(&data.collision_walls, wall);
            } break;

            case DUNGEON_TILE_TYPE_PLAYER_POSITION: {
                data.player_position = {(F32)(curr_x) * DUNGEON_TILE_SIZE, (F32)(curr_y) * DUNGEON_TILE_SIZE};
                c3d_set_position({data.player_position.x, 0.0F, data.player_position.y}); // TODO: This needs to happen elsewhere later.
            } break;

            default: {
                _unreachable_();
            } break;
        }

        ptr++;
    }

    // Generate combined model with multiple meshes from all tiles
    SZ const tile_count = data.tiles.count;

    if (tile_count > 0) {
        SZ const MAX_TILES_PER_MESH = 2700; // ~65k vertices max per mesh

        // Sort tiles by spatial location to keep adjacent tiles in same mesh
        // This prevents seams between meshes at tile boundaries
        for (SZ i = 0; i < tile_count - 1; i++) {
            for (SZ j = i + 1; j < tile_count; j++) {
                IDungeonTileData* a = &data.tiles.data[i];
                IDungeonTileData* b = &data.tiles.data[j];

                // Sort by Z first, then X (row-major order)
                F32 const a_key = (a->position.z * 1000.0F) + a->position.x;
                F32 const b_key = (b->position.z * 1000.0F) + b->position.x;

                if (a_key > b_key) {
                    IDungeonTileData const temp = *a;
                    *a = *b;
                    *b = temp;
                }
            }
        }

        SZ const mesh_count = (tile_count + MAX_TILES_PER_MESH - 1) / MAX_TILES_PER_MESH;

        // Initialize model
        data.combined_model.meshCount     = (S32)mesh_count;
        data.combined_model.materialCount = 1;
        data.combined_model.meshes        = mmpa(Mesh*, mesh_count * sizeof(Mesh));
        data.combined_model.materials     = mmpa(Material*, sizeof(Material));
        data.combined_model.meshMaterial  = mmpa(S32*, mesh_count * sizeof(S32));
        data.combined_model.transform     = MatrixIdentity();

        // Use default material with rock texture
        data.combined_model.materials[0]        = g_render.default_material;
        data.combined_model.materials[0].shader = asset_get_shader(A_MODEL_SHADER_NAME)->base;

        // Load and apply rock texture
        ATexture *texture = asset_get_texture("cracked_mud.jpg");
        if (texture) {
            data.combined_model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture->base;
        }
        for (SZ i = 0; i < mesh_count; i++) {
            data.combined_model.meshMaterial[i] = 0;
        }

        // Create each mesh
        for (SZ mesh_idx = 0; mesh_idx < mesh_count; mesh_idx++) {
            SZ const start_tile = mesh_idx * MAX_TILES_PER_MESH;
            SZ const end_tile   = (start_tile + MAX_TILES_PER_MESH < tile_count) ? (start_tile + MAX_TILES_PER_MESH) : tile_count;

            // Pre-count merged shapes to allocate correct memory
            SZ merged_shape_count = 0;
            for (SZ tile_idx = start_tile; tile_idx < end_tile; ) {
                IDungeonTileData* tile = &data.tiles.data[tile_idx];
                SZ run_end             = tile_idx + 1;

                while (run_end < end_tile) {
                    IDungeonTileData* next_tile = &data.tiles.data[run_end];
                    if (next_tile->type == tile->type &&
                        next_tile->tint.r == tile->tint.r && next_tile->tint.g == tile->tint.g &&
                        next_tile->tint.b == tile->tint.b && next_tile->tint.a == tile->tint.a &&
                        next_tile->size.x == tile->size.x && next_tile->size.y == tile->size.y && next_tile->size.z == tile->size.z &&
                        next_tile->position.y == tile->position.y && next_tile->position.z == tile->position.z &&
                        next_tile->position.x == data.tiles.data[run_end - 1].position.x + tile->size.x) {
                        run_end++;
                    } else {
                        break;
                    }
                }

                merged_shape_count++;
                tile_idx = run_end;
            }

            // Allocate mesh with worst-case size (will resize after face extraction)
            Mesh* mesh = &data.combined_model.meshes[mesh_idx];

            // Extract and merge faces using greedy meshing
            SZ face_count = 0;
            struct Face {
                Vector3 corners[4];  // 4 corners of quad
                Vector3 normal;
                Color color;
            };
            Face* faces = mmpa(Face*, merged_shape_count * 6 * sizeof(Face)); // worst case: 6 faces per shape

            // Generate faces for each tile, merging adjacent ones
            for (SZ tile_idx = start_tile; tile_idx < end_tile; tile_idx++) {
                IDungeonTileData* tile = &data.tiles.data[tile_idx];
                Vector3 const pos      = tile->position;
                Vector3 const size     = tile->size;
                Color const color      = tile->tint;
                F32 const half_x       = size.x/2;
                F32 const half_z       = size.z/2;

                // Check which faces are exposed (not hidden by adjacent tiles)
                BOOL front_exposed  = true;
                BOOL back_exposed   = true;
                BOOL top_exposed    = true;
                BOOL bottom_exposed = true;
                BOOL right_exposed  = true;
                BOOL left_exposed   = true;

                // Check adjacency (simplified - just check if there's another tile at adjacent position)
                for (SZ other_idx = start_tile; other_idx < end_tile; other_idx++) {
                    if (other_idx == tile_idx) { continue; }
                    IDungeonTileData* other = &data.tiles.data[other_idx];

                    if (other->type == tile->type &&
                        other->size.x == size.x && other->size.y == size.y && other->size.z == size.z) {

                        Vector3 const diff = {other->position.x - pos.x, other->position.y - pos.y, other->position.z - pos.z};

                        if (math_abs_f32(diff.y) < 0.01F && math_abs_f32(diff.z) < 0.01F) {
                            if (math_abs_f32(diff.x - size.x) < 0.01F) { right_exposed = false; }
                            if (math_abs_f32(diff.x + size.x) < 0.01F) { left_exposed = false; }
                        }

                        if (math_abs_f32(diff.x) < 0.01F && math_abs_f32(diff.z) < 0.01F) {
                            if (math_abs_f32(diff.y - size.y) < 0.01F) { top_exposed = false; }
                            if (math_abs_f32(diff.y + size.y) < 0.01F) { bottom_exposed = false; }
                        }

                        if (math_abs_f32(diff.x) < 0.01F && math_abs_f32(diff.y) < 0.01F) {
                            if (math_abs_f32(diff.z - size.z) < 0.01F) { front_exposed = false; }
                            if (math_abs_f32(diff.z + size.z) < 0.01F) { back_exposed = false; }
                        }
                    }
                }

                // Add exposed faces
                if (front_exposed) {
                    faces[face_count].corners[0] = {pos.x - half_x, pos.y, pos.z + half_z};
                    faces[face_count].corners[1] = {pos.x + half_x, pos.y, pos.z + half_z};
                    faces[face_count].corners[2] = {pos.x + half_x, pos.y + size.y, pos.z + half_z};
                    faces[face_count].corners[3] = {pos.x - half_x, pos.y + size.y, pos.z + half_z};
                    faces[face_count].normal = {0.0F, 0.0F, 1.0F};
                    faces[face_count].color = color;
                    face_count++;
                }

                if (back_exposed) {
                    faces[face_count].corners[0] = {pos.x + half_x, pos.y, pos.z - half_z};
                    faces[face_count].corners[1] = {pos.x - half_x, pos.y, pos.z - half_z};
                    faces[face_count].corners[2] = {pos.x - half_x, pos.y + size.y, pos.z - half_z};
                    faces[face_count].corners[3] = {pos.x + half_x, pos.y + size.y, pos.z - half_z};
                    faces[face_count].normal = {0.0F, 0.0F, -1.0F};
                    faces[face_count].color = color;
                    face_count++;
                }

                if (top_exposed) {
                    faces[face_count].corners[0] = {pos.x - half_x, pos.y + size.y, pos.z - half_z};
                    faces[face_count].corners[1] = {pos.x - half_x, pos.y + size.y, pos.z + half_z};
                    faces[face_count].corners[2] = {pos.x + half_x, pos.y + size.y, pos.z + half_z};
                    faces[face_count].corners[3] = {pos.x + half_x, pos.y + size.y, pos.z - half_z};
                    faces[face_count].normal = {0.0F, 1.0F, 0.0F};
                    faces[face_count].color = color;
                    face_count++;
                }

                if (bottom_exposed) {
                    faces[face_count].corners[0] = {pos.x - half_x, pos.y, pos.z + half_z};
                    faces[face_count].corners[1] = {pos.x - half_x, pos.y, pos.z - half_z};
                    faces[face_count].corners[2] = {pos.x + half_x, pos.y, pos.z - half_z};
                    faces[face_count].corners[3] = {pos.x + half_x, pos.y, pos.z + half_z};
                    faces[face_count].normal = {0.0F, -1.0F, 0.0F};
                    faces[face_count].color = color;
                    face_count++;
                }

                if (right_exposed) {
                    faces[face_count].corners[0] = {pos.x + half_x, pos.y, pos.z + half_z};
                    faces[face_count].corners[1] = {pos.x + half_x, pos.y, pos.z - half_z};
                    faces[face_count].corners[2] = {pos.x + half_x, pos.y + size.y, pos.z - half_z};
                    faces[face_count].corners[3] = {pos.x + half_x, pos.y + size.y, pos.z + half_z};
                    faces[face_count].normal = {1.0F, 0.0F, 0.0F};
                    faces[face_count].color = color;
                    face_count++;
                }

                if (left_exposed) {
                    faces[face_count].corners[0] = {pos.x - half_x, pos.y, pos.z - half_z};
                    faces[face_count].corners[1] = {pos.x - half_x, pos.y, pos.z + half_z};
                    faces[face_count].corners[2] = {pos.x - half_x, pos.y + size.y, pos.z + half_z};
                    faces[face_count].corners[3] = {pos.x - half_x, pos.y + size.y, pos.z - half_z};
                    faces[face_count].normal = {-1.0F, 0.0F, 0.0F};
                    faces[face_count].color = color;
                    face_count++;
                }
            }

            // Allocate mesh memory based on actual face count
            mesh->vertexCount   = (S32)(face_count * 4);
            mesh->triangleCount = (S32)(face_count * 2);
            mesh->vertices      = mmpa(F32*, face_count * 4 * 3 * sizeof(F32));
            mesh->normals       = mmpa(F32*, face_count * 4 * 3 * sizeof(F32));
            mesh->texcoords     = mmpa(F32*, face_count * 4 * 2 * sizeof(F32));
            mesh->indices       = mmpa(U16*, face_count * 6 * sizeof(U16));
            mesh->colors        = mmpa(U8*, face_count * 4 * 4 * sizeof(U8));

            // Generate mesh from faces
            for (SZ face_idx = 0; face_idx < face_count; face_idx++) {
                Face* face = &faces[face_idx];

                // Add 4 vertices for this face
                for (SZ i = 0; i < 4; i++) {
                    SZ const vert_idx = (face_idx * 4 + i) * 3;
                    mesh->vertices[vert_idx + 0] = face->corners[i].x;
                    mesh->vertices[vert_idx + 1] = face->corners[i].y;
                    mesh->vertices[vert_idx + 2] = face->corners[i].z;

                    mesh->normals[vert_idx + 0] = face->normal.x;
                    mesh->normals[vert_idx + 1] = face->normal.y;
                    mesh->normals[vert_idx + 2] = face->normal.z;
                }

                // Add texture coordinates
                SZ const tex_idx = face_idx * 4 * 2;
                F32 const tex_coords[] = {0.0F, 1.0F, 1.0F, 1.0F, 1.0F, 0.0F, 0.0F, 0.0F};
                for (SZ i = 0; i < 8; i++) {
                    mesh->texcoords[tex_idx + i] = tex_coords[i];
                }

                // Add colors
                for (SZ i = 0; i < 4; i++) {
                    SZ const color_idx = (face_idx * 4 + i) * 4;
                    mesh->colors[color_idx + 0] = face->color.r;
                    mesh->colors[color_idx + 1] = face->color.g;
                    mesh->colors[color_idx + 2] = face->color.b;
                    mesh->colors[color_idx + 3] = face->color.a;
                }

                // Add indices for 2 triangles
                SZ const idx_base = face_idx * 4;
                SZ const indices_idx = face_idx * 6;
                mesh->indices[indices_idx + 0] = (U16)(idx_base + 0);
                mesh->indices[indices_idx + 1] = (U16)(idx_base + 1);
                mesh->indices[indices_idx + 2] = (U16)(idx_base + 2);
                mesh->indices[indices_idx + 3] = (U16)(idx_base + 2);
                mesh->indices[indices_idx + 4] = (U16)(idx_base + 3);
                mesh->indices[indices_idx + 5] = (U16)(idx_base + 0);
            }

            // Upload this mesh to GPU
            UploadMesh(mesh, false);
        }
    }

    return data;
}

IDungeonData static g_dungeon_data = {};
BOOL static g_dungeon_loaded = false;

BOOL static i_is_position_on_floor(Vector3 position) {
    F32 const tile_size = DUNGEON_TILE_SIZE;
    F32 const half_tile = tile_size / 2.0F;

    for (SZ i = 0; i < g_dungeon_data.tiles.count; i++) {
        IDungeonTileData *tile = &g_dungeon_data.tiles.data[i];
        if (tile->type == DUNGEON_TILE_TYPE_BASIC_FLOOR) {
            // Check if player position is within the floor tile bounds
            F32 const tile_min_x = tile->position.x - half_tile;
            F32 const tile_max_x = tile->position.x + half_tile;
            F32 const tile_min_z = tile->position.z - half_tile;
            F32 const tile_max_z = tile->position.z + half_tile;

            if (position.x >= tile_min_x && position.x <= tile_max_x &&
                position.z >= tile_min_z && position.z <= tile_max_z &&
                position.y >= tile->position.y &&
                position.y <= tile->position.y + tile->size.y + PLAYER_HEIGHT_TO_EYES) {
                return true;
            }
        }
    }
    return false;
}

void dungeon_draw_3d_sketch() {
    if (!g_dungeon_loaded) {
        g_dungeon_data = i_parse_dungeon("assets/dungeons/example.dun");
        g_dungeon_loaded = true;
    }

    // Draw the combined model (which contains multiple meshes)
    if (g_dungeon_data.combined_model.meshCount > 0) {
        d3d_model_rl(&g_dungeon_data.combined_model, Vector3Zero(), 1.0F, WHITE);
    }
}

// Check if position would be too close to any wall
BOOL static i_is_position_too_close_to_wall(Vector3 pos) {
    for (SZ i = 0; i < g_dungeon_data.collision_walls.count; i++) {
        IDungeonCollisionWall *wall = &g_dungeon_data.collision_walls.data[i];

        // Calculate distance from player position to wall center
        F32 const dist_x = math_abs_f32(pos.x - wall->position.x);
        F32 const dist_z = math_abs_f32(pos.z - wall->position.z);

        // Check if we're too close (wall half-size + buffer distance)
        F32 const wall_half_size = DUNGEON_TILE_SIZE * 0.5F;
        F32 const min_dist_x     = wall_half_size + WALL_BUFFER_DISTANCE;
        F32 const min_dist_z     = wall_half_size + WALL_BUFFER_DISTANCE;

        if (dist_x < min_dist_x && dist_z < min_dist_z) {
            return true;
        }
    }
    return false;
}

void dungeon_update(Player *player, F32 dt) {
    unused(dt);

    Vector3 static prev_player_pos  = {0, 0, 0};
    Vector3 static initialized      = {0, 0, 0};
    F32 static accumulated_distance = 0.0F;
    BOOL static was_moving          = false;
    BOOL static was_noclip          = false;

    if (c_debug__noclip) {
        was_noclip = true;
        return;
    }

    // Reset position tracking when coming out of noclip
    if (was_noclip) {
        prev_player_pos      = player->cameras[g_scenes.current_scene_type].position;
        was_moving           = false;
        accumulated_distance = 0.0F;
        was_noclip           = false;
        return;
    }

    Vector3 const current_pos = player->cameras[g_scenes.current_scene_type].position;
    Vector3 const player_size = {PLAYER_RADIUS * 2, PLAYER_HEIGHT_TO_EYES, PLAYER_RADIUS * 2};

    // Initialize previous position on first call
    if (initialized.x == 0) {
        prev_player_pos = current_pos;
        initialized.x   = 1;
        return;
    }

    // Check if player has moved
    Vector3 const movement      = Vector3Subtract(current_pos, prev_player_pos);
    F32 const movement_distance = Vector3Length(movement);
    BOOL const is_moving        = movement_distance > 0.001F;

    // Play footstep on movement start
    if (is_moving && !was_moving && i_is_position_on_floor(current_pos)) {
        player_step();
        accumulated_distance = 0.0F;
    }

    if (!is_moving) {
        was_moving = false;
        return; // No movement, no collision check needed
    }

    // Track distance for footsteps during continuous movement
    accumulated_distance += movement_distance;
    if (accumulated_distance >= FOOTSTEP_DISTANCE_THRESHOLD) {
        if (i_is_position_on_floor(current_pos)) {
            player_step();
        }
        accumulated_distance = 0.0F;
    }

    // Simple grid collision - try X movement first, then Z movement
    Vector3 new_position = current_pos;  // Start with current position to keep Y

    // Try X movement
    Vector3 const test_pos_x = {prev_player_pos.x + movement.x, current_pos.y, prev_player_pos.z};
    if (!i_is_position_too_close_to_wall(test_pos_x)) {
        new_position.x = test_pos_x.x;
    } else {
        new_position.x = prev_player_pos.x;  // Keep old X if blocked
    }

    // Try Z movement
    Vector3 const test_pos_z = {new_position.x, current_pos.y, prev_player_pos.z + movement.z};
    if (!i_is_position_too_close_to_wall(test_pos_z)) {
        new_position.z = test_pos_z.z;
    } else {
        new_position.z = prev_player_pos.z;  // Keep old Z if blocked
    }

    // Apply the final position
    Vector3 const pos_delta   = Vector3Subtract(new_position, current_pos);
    player->cameras[g_scenes.current_scene_type].position = Vector3Add(player->cameras[g_scenes.current_scene_type].position, pos_delta);
    player->cameras[g_scenes.current_scene_type].target   = Vector3Add(player->cameras[g_scenes.current_scene_type].target, pos_delta);

    prev_player_pos = new_position;
    was_moving      = is_moving;
}

void dungeon_draw_2d_dbg() {
    if (!c_debug__dungeon_info) { return; }

    // Small minimap in top-left corner
    Vector2 const map_pos      = {5.0F, 5.0F};
    F32 const tile_screen_size = (F32)(S32)ui_scale_x(0.25F);
    Color const wall_color     = g_render.sketch.minor_color;
    Color const floor_color    = g_render.sketch.major_color;

    // Draw tiles - just go back to individual tiles, batching isn't worth the complexity
    for (SZ i = 0; i < g_dungeon_data.tiles.count; i++) {
        IDungeonTileData *tile = &g_dungeon_data.tiles.data[i];

        Vector2 const screen_pos = {
            map_pos.x + ((tile->position.x / DUNGEON_TILE_SIZE) * tile_screen_size),
            map_pos.y + ((tile->position.z / DUNGEON_TILE_SIZE) * tile_screen_size),
        };

        if (tile->type == DUNGEON_TILE_TYPE_BASIC_FLOOR) {
            d2d_rectangle_rec({screen_pos.x, screen_pos.y, tile_screen_size, tile_screen_size}, floor_color);
        } else if (tile->type == DUNGEON_TILE_TYPE_BASIC_WALL) {
            d2d_rectangle_rec({screen_pos.x, screen_pos.y, tile_screen_size, tile_screen_size}, wall_color);
        }
    }

    // Camera frustum visualization
    Camera3D const camera    = c3d_get();
    Vector3 const cam_pos    = camera.position;
    Vector3 const cam_target = camera.target;
    F32 const fov_rad        = c3d_get_fov() * DEG2RAD;
    F32 const aspect         = g_render.aspect_ratio;

    // Convert camera position to screen using same formula as tiles
    Vector2 const cam_screen_pos = {
        map_pos.x + ((cam_pos.x / DUNGEON_TILE_SIZE) * tile_screen_size),
        map_pos.y + ((cam_pos.z / DUNGEON_TILE_SIZE) * tile_screen_size),
    };

    // Calculate view cone parameters
    Vector3 const forward       = Vector3Normalize(Vector3Subtract(cam_target, cam_pos));
    F32 const max_view_distance = 60.0F;
    F32 const half_fov          = fov_rad * 0.5F * aspect;

    // Calculate the angle of the forward direction in 2D (top-down)
    F32 const forward_angle = math_atan2_f32(forward.z, forward.x);

    // Just use max distance for the cone - don't shrink it based on walls
    F32 const screen_radius = (max_view_distance / DUNGEON_TILE_SIZE) * tile_screen_size;
    F32 const start_angle   = (forward_angle - half_fov) * RAD2DEG;
    F32 const end_angle     = (forward_angle + half_fov) * RAD2DEG;

    // Draw the view cone as a proper circular sector
    Color const cone_fill_color = Fade(YELLOW, 0.2F);
    Color const cone_edge_color = Fade(YELLOW, 0.8F);

    d2d_circle_sector(cam_screen_pos, screen_radius, start_angle, end_angle, 16, cone_fill_color);
    d2d_circle_sector_lines(cam_screen_pos, screen_radius, start_angle, end_angle, 16, cone_edge_color);

    // Draw camera position with direction indicator
    d2d_circle(cam_screen_pos, 4.0F, RED);
    d2d_circle(cam_screen_pos, 2.0F, WHITE);

    // Camera direction line
    Vector2 const cam_forward_screen = {
        cam_screen_pos.x + (forward.x * tile_screen_size * 0.1F),
        cam_screen_pos.y + (forward.z * tile_screen_size * 0.1F),
    };
    d2d_line(cam_screen_pos, cam_forward_screen, WHITE);
}
