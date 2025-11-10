#include "dungeon.hpp"
#include <stdio.h>
#include "array.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "common.hpp"
#include "cvar.hpp"
#include "math.hpp"
#include "memory.hpp"
#include "raymath.h"
#include "render.hpp"
#include "player.hpp"
#include "world.hpp"

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
    OrientedBoundingBox *mesh_bounding_boxes;  // One per mesh for frustum culling
    SZ mesh_bbox_count;
    S32 *wall_grid;     // 2D grid for fast wall lookups (wall index or -1 for no wall)
    S32 grid_min_x;
    S32 grid_min_z;
    S32 grid_max_x;
    S32 grid_max_z;
    S32 grid_width;
    S32 grid_height;
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

    // Build spatial grid for fast wall lookups
    SZ const tile_count = data.tiles.count;
    if (tile_count > 0) {
        // Find grid bounds
        data.grid_min_x = S32_MAX;
        data.grid_min_z = S32_MAX;
        data.grid_max_x = S32_MIN;
        data.grid_max_z = S32_MIN;

        F32 const half_tile = tile_size * 0.5F;
        for (SZ i = 0; i < tile_count; i++) {
            // Tiles are centered, offset by half to get grid-aligned coordinates
            S32 const grid_x = (S32)math_floor_f32((data.tiles.data[i].position.x + half_tile) / tile_size);
            S32 const grid_z = (S32)math_floor_f32((data.tiles.data[i].position.z + half_tile) / tile_size);
            data.grid_min_x = glm::min(data.grid_min_x, grid_x);
            data.grid_min_z = glm::min(data.grid_min_z, grid_z);
            data.grid_max_x = glm::max(data.grid_max_x, grid_x);
            data.grid_max_z = glm::max(data.grid_max_z, grid_z);
        }

        data.grid_width  = data.grid_max_x - data.grid_min_x + 1;
        data.grid_height = data.grid_max_z - data.grid_min_z + 1;

        // Allocate and initialize the grid with -1 (no wall)
        SZ const grid_size = (SZ)data.grid_width * (SZ)data.grid_height;
        data.wall_grid = mmpa(S32*, grid_size * sizeof(S32));
        for (SZ i = 0; i < grid_size; i++) {
            data.wall_grid[i] = -1;
        }

        // Mark wall cells with their index
        for (SZ i = 0; i < data.collision_walls.count; i++) {
            IDungeonCollisionWall const *wall = &data.collision_walls.data[i];
            // Walls are centered, offset by half to get grid-aligned coordinates
            S32 const grid_x = (S32)math_floor_f32((wall->position.x + half_tile) / tile_size);
            S32 const grid_z = (S32)math_floor_f32((wall->position.z + half_tile) / tile_size);
            S32 const local_x = grid_x - data.grid_min_x;
            S32 const local_z = grid_z - data.grid_min_z;

            if (local_x >= 0 && local_x < data.grid_width && local_z >= 0 && local_z < data.grid_height) {
                data.wall_grid[(local_z * data.grid_width) + local_x] = (S32)i;
            }
        }

        // Generate combined model with multiple meshes from all tiles
        SZ const MAX_TILES_PER_MESH = 300; // Smaller chunks for better frustum culling (face culling fixed for boundaries)

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

        // Allocate bounding boxes for frustum culling
        data.mesh_bounding_boxes = mmpa(OrientedBoundingBox*, mesh_count * sizeof(OrientedBoundingBox));
        data.mesh_bbox_count = mesh_count;

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

                // Check adjacency across ALL tiles (not just current mesh) for proper face culling at boundaries
                for (SZ other_idx = 0; other_idx < tile_count; other_idx++) {
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

            // Calculate bounding box for this mesh
            Vector3 min_bounds = {F32_MAX, F32_MAX, F32_MAX};
            Vector3 max_bounds = {-F32_MAX, -F32_MAX, -F32_MAX};

            for (SZ tile_idx = start_tile; tile_idx < end_tile; tile_idx++) {
                IDungeonTileData* tile = &data.tiles.data[tile_idx];
                Vector3 const pos = tile->position;
                Vector3 const size = tile->size;

                // Calculate tile bounds
                Vector3 const tile_min = {pos.x - (size.x/2), pos.y, pos.z - (size.z/2)};
                Vector3 const tile_max = {pos.x + (size.x/2), pos.y + size.y, pos.z + (size.z/2)};

                // Update mesh bounds
                min_bounds.x = glm::min(min_bounds.x, tile_min.x);
                min_bounds.y = glm::min(min_bounds.y, tile_min.y);
                min_bounds.z = glm::min(min_bounds.z, tile_min.z);
                max_bounds.x = glm::max(max_bounds.x, tile_max.x);
                max_bounds.y = glm::max(max_bounds.y, tile_max.y);
                max_bounds.z = glm::max(max_bounds.z, tile_max.z);
            }

            // Create axis-aligned OBB
            data.mesh_bounding_boxes[mesh_idx].center = {
                (min_bounds.x + max_bounds.x) * 0.5F,
                (min_bounds.y + max_bounds.y) * 0.5F,
                (min_bounds.z + max_bounds.z) * 0.5F
            };
            data.mesh_bounding_boxes[mesh_idx].extents = {
                (max_bounds.x - min_bounds.x) * 0.5F,
                (max_bounds.y - min_bounds.y) * 0.5F,
                (max_bounds.z - min_bounds.z) * 0.5F
            };
            // Axis-aligned, so axes are identity
            data.mesh_bounding_boxes[mesh_idx].axes[0] = {1.0F, 0.0F, 0.0F};
            data.mesh_bounding_boxes[mesh_idx].axes[1] = {0.0F, 1.0F, 0.0F};
            data.mesh_bounding_boxes[mesh_idx].axes[2] = {0.0F, 0.0F, 1.0F};
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

void dungeon_update(F32 dt) {
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
        prev_player_pos      = g_player.cameras[g_scenes.current_scene_type].position;
        was_moving           = false;
        accumulated_distance = 0.0F;
        was_noclip           = false;
        return;
    }

    Vector3 const current_pos = g_player.cameras[g_scenes.current_scene_type].position;
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
    g_player.cameras[g_scenes.current_scene_type].position = Vector3Add(g_player.cameras[g_scenes.current_scene_type].position, pos_delta);
    g_player.cameras[g_scenes.current_scene_type].target   = Vector3Add(g_player.cameras[g_scenes.current_scene_type].target, pos_delta);

    prev_player_pos = new_position;
    was_moving      = is_moving;
}

BOOL dungeon_is_entity_occluded(EID entity_id) {
    if (!g_dungeon_loaded) { return false; }

    Camera3D const *cam = c3d_get_ptr();
    Vector3 const camera_pos = cam->position;
    Vector3 const entity_pos = g_world->position[entity_id];

    // Get entity size for tolerance (use largest horizontal extent)
    F32 const entity_radius = glm::max(g_world->obb[entity_id].extents.x, g_world->obb[entity_id].extents.z);

    // Create ray from camera to entity
    Vector3 const ray_dir = Vector3Subtract(entity_pos, camera_pos);
    F32 const ray_length = Vector3Length(ray_dir);
    Vector3 const ray_direction = Vector3Scale(ray_dir, 1.0F / ray_length);

    F32 const tile_size = DUNGEON_TILE_SIZE;
    F32 const half_tile = tile_size * 0.5F;

    // Use Bresenham to traverse grid and only check walls near the ray path
    S32 const x0 = (S32)math_floor_f32((camera_pos.x + half_tile) / tile_size);
    S32 const z0 = (S32)math_floor_f32((camera_pos.z + half_tile) / tile_size);
    S32 const x1 = (S32)math_floor_f32((entity_pos.x + half_tile) / tile_size);
    S32 const z1 = (S32)math_floor_f32((entity_pos.z + half_tile) / tile_size);

    S32 dx = math_abs_s32(x1 - x0);
    S32 dz = math_abs_s32(z1 - z0);
    S32 sx = (x0 < x1) ? 1 : -1;
    S32 sz = (z0 < z1) ? 1 : -1;
    S32 err = dx - dz;

    S32 x = x0;
    S32 z = z0;

    // Precompute inverse direction for ray-box tests
    Vector3 const inv_dir = {
        math_abs_f32(ray_direction.x) > 0.0001F ? 1.0F / ray_direction.x : 1e6F,
        math_abs_f32(ray_direction.y) > 0.0001F ? 1.0F / ray_direction.y : 1e6F,
        math_abs_f32(ray_direction.z) > 0.0001F ? 1.0F / ray_direction.z : 1e6F
    };

    while (true) {
        // Reached the entity's tile
        if (x == x1 && z == z1) { break; }

        // Skip the starting tile (camera position)
        if (x != x0 || z != z0) {
            // Fast grid lookup
            S32 const local_x = x - g_dungeon_data.grid_min_x;
            S32 const local_z = z - g_dungeon_data.grid_min_z;

            // Check if this grid cell has a wall
            if (local_x >= 0 && local_x < g_dungeon_data.grid_width &&
                local_z >= 0 && local_z < g_dungeon_data.grid_height) {
                S32 const wall_idx = g_dungeon_data.wall_grid[(local_z * g_dungeon_data.grid_width) + local_x];

                if (wall_idx >= 0) {
                    // Direct O(1) lookup of the wall
                    IDungeonCollisionWall const *wall = &g_dungeon_data.collision_walls.data[wall_idx];

                    // Expand wall AABB by entity radius for tolerance
                    Vector3 const wall_min = {
                        wall->position.x - (wall->size.x * 0.5F) - entity_radius,
                        wall->position.y,
                        wall->position.z - (wall->size.z * 0.5F) - entity_radius
                    };
                    Vector3 const wall_max = {
                        wall->position.x + (wall->size.x * 0.5F) + entity_radius,
                        wall->position.y + wall->size.y,
                        wall->position.z + (wall->size.z * 0.5F) + entity_radius
                    };

                    // Ray-AABB intersection test
                    F32 const t1 = (wall_min.x - camera_pos.x) * inv_dir.x;
                    F32 const t2 = (wall_max.x - camera_pos.x) * inv_dir.x;
                    F32 const t3 = (wall_min.y - camera_pos.y) * inv_dir.y;
                    F32 const t4 = (wall_max.y - camera_pos.y) * inv_dir.y;
                    F32 const t5 = (wall_min.z - camera_pos.z) * inv_dir.z;
                    F32 const t6 = (wall_max.z - camera_pos.z) * inv_dir.z;

                    F32 const tmin = glm::max(glm::max(glm::min(t1, t2), glm::min(t3, t4)), glm::min(t5, t6));
                    F32 const tmax = glm::min(glm::min(glm::max(t1, t2), glm::max(t3, t4)), glm::max(t5, t6));

                    // Ray intersects box if tmax >= tmin and tmin < ray_length
                    if (tmax >= tmin && tmin >= 0.0F && tmin < ray_length) {
                        return true; // Wall blocks line of sight
                    }
                }
            }
        }

        // Bresenham step
        S32 const e2 = 2 * err;
        if (e2 > -dz) {
            err -= dz;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            z += sz;
        }
    }

    return false; // No walls blocking line of sight
}

BOOL static i_check_position_collision(Vector3 pos, F32 radius) {
    for (SZ i = 0; i < g_dungeon_data.collision_walls.count; i++) {
        IDungeonCollisionWall *wall = &g_dungeon_data.collision_walls.data[i];

        // Calculate distance from position to wall center
        F32 const dist_x = math_abs_f32(pos.x - wall->position.x);
        F32 const dist_z = math_abs_f32(pos.z - wall->position.z);

        // Check if we're too close (wall half-size + entity radius)
        F32 const wall_half_size = DUNGEON_TILE_SIZE * 0.5F;
        F32 const min_dist_x     = wall_half_size + radius;
        F32 const min_dist_z     = wall_half_size + radius;

        if (dist_x < min_dist_x && dist_z < min_dist_z) {
            return true; // Colliding with wall
        }
    }
    return false;
}

Vector3 dungeon_resolve_wall_collision(EID entity_id, Vector3 desired_pos) {
    if (!g_dungeon_loaded) { return desired_pos; }

    Vector3 const current_pos = g_world->position[entity_id];
    F32 const radius = glm::max(g_world->obb[entity_id].extents.x, g_world->obb[entity_id].extents.z);

    // If desired position doesn't collide, use it
    if (!i_check_position_collision(desired_pos, radius)) {
        return desired_pos;
    }

    // Collision detected - try sliding along X or Z axis
    Vector3 slide_x = {desired_pos.x, desired_pos.y, current_pos.z}; // Slide along X only
    Vector3 slide_z = {current_pos.x, desired_pos.y, desired_pos.z}; // Slide along Z only

    BOOL const x_valid = !i_check_position_collision(slide_x, radius);
    BOOL const z_valid = !i_check_position_collision(slide_z, radius);

    // Return the valid slide direction, prefer whichever moved us further
    if (x_valid && z_valid) {
        // Both valid - pick the one with more movement
        F32 const x_dist = math_abs_f32(desired_pos.x - current_pos.x);
        F32 const z_dist = math_abs_f32(desired_pos.z - current_pos.z);
        return (x_dist > z_dist) ? slide_x : slide_z;
    }

    if (x_valid) { return slide_x; }
    if (z_valid) { return slide_z; }

    // Neither slide works - stay in place
    return current_pos;
}

void dungeon_draw_3d_sketch() {
    if (!g_dungeon_loaded) {
        g_dungeon_data = i_parse_dungeon("assets/dungeons/example.dun");
        g_dungeon_loaded = true;
    }

    // Draw each mesh individually with frustum culling
    if (g_dungeon_data.combined_model.meshCount > 0) {
        Matrix transform = MatrixIdentity();
        Material *material = &g_dungeon_data.combined_model.materials[0];

        for (S32 i = 0; i < g_dungeon_data.combined_model.meshCount; i++) {
            // Frustum cull this mesh
            if (c3d_is_obb_in_frustum(g_dungeon_data.mesh_bounding_boxes[i])) {
                d3d_mesh_rl(&g_dungeon_data.combined_model.meshes[i], material, &transform);
            }
        }
    }
}

void dungeon_draw_2d_dbg() {
    if (!c_debug__dungeon_info) { return; }

    // Small minimap in top-left corner
    Vector2 const map_pos      = {5.0F, 5.0F};
    F32 const tile_screen_size = (F32)(S32)ui_scale_x(0.25F);
    Color const wall_color     = g_render.sketch_shader.minor_color;
    Color const floor_color    = g_render.sketch_shader.major_color;

    // Merge adjacent tiles into larger rectangles to reduce draw calls
    Rectangle *merged_rects = mmta(Rectangle*, g_dungeon_data.tiles.count * sizeof(Rectangle));
    Color *merged_colors = mmta(Color*, g_dungeon_data.tiles.count * sizeof(Color));
    SZ merged_count = 0;

    for (SZ i = 0; i < g_dungeon_data.tiles.count; i++) {
        IDungeonTileData *tile = &g_dungeon_data.tiles.data[i];

        if (tile->type != DUNGEON_TILE_TYPE_BASIC_FLOOR && tile->type != DUNGEON_TILE_TYPE_BASIC_WALL) {
            continue;
        }

        Color const color = (tile->type == DUNGEON_TILE_TYPE_BASIC_FLOOR) ? floor_color : wall_color;

        F32 const start_x = tile->position.x;
        F32 const z = tile->position.z;

        // Merge horizontally: find consecutive tiles on same row with same type
        SZ run_length = 1;
        while (i + run_length < g_dungeon_data.tiles.count) {
            IDungeonTileData *next = &g_dungeon_data.tiles.data[i + run_length];

            // Same type, same row, consecutive X position?
            if (next->type == tile->type &&
                next->position.z == z &&
                next->position.x == start_x + ((F32)run_length * DUNGEON_TILE_SIZE)) {
                run_length++;
            } else {
                break;
            }
        }

        // Create merged rectangle
        Vector2 const screen_start = {
            map_pos.x + ((start_x / DUNGEON_TILE_SIZE) * tile_screen_size),
            map_pos.y + ((z / DUNGEON_TILE_SIZE) * tile_screen_size),
        };

        merged_rects[merged_count] = {
            screen_start.x,
            screen_start.y,
            tile_screen_size * (F32)run_length,
            tile_screen_size
        };
        merged_colors[merged_count] = color;
        merged_count++;

        // Skip the tiles we just merged
        i += run_length - 1;
    }

    // Draw merged rectangles
    for (SZ i = 0; i < merged_count; i++) {
        d2d_rectangle_rec(merged_rects[i], merged_colors[i]);
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

    // Draw entities
    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        if (!ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_IN_USE)) { continue; }

        Vector3 const entity_pos = g_world->position[i];

        // Convert entity position to screen coordinates
        Vector2 const entity_screen_pos = {
            map_pos.x + ((entity_pos.x / DUNGEON_TILE_SIZE) * tile_screen_size),
            map_pos.y + ((entity_pos.z / DUNGEON_TILE_SIZE) * tile_screen_size),
        };

        // Check frustum first (cheap flag check)
        BOOL const in_frustum = ENTITY_HAS_FLAG(g_world->flags[i], ENTITY_FLAG_IN_FRUSTUM);

        // Color based on occlusion and frustum status
        Color entity_color;
        if (!in_frustum) {
            entity_color = GRAY; // Not in frustum - skip expensive occlusion check
        } else {
            // Only check occlusion for entities in frustum
            BOOL const is_occluded = dungeon_is_entity_occluded(i);
            entity_color = is_occluded ? RED : GREEN;
        }

        // Draw entity as a circle
        d2d_circle(entity_screen_pos, 3.0F, entity_color);
    }
}
