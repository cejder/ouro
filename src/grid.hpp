#pragma once

#include "common.hpp"
#include "entity.hpp"

#include <raylib.h>

#define GRID_CELLS_PER_ROW 150
#define GRID_TOTAL_CELLS (GRID_CELLS_PER_ROW * GRID_CELLS_PER_ROW)
#define GRID_MAX_ENTITIES_PER_CELL 50
#define GRID_NEARBY_ENTITIES_MAX 64

fwd_decl(World);

struct GridCell {
    EID entities[GRID_MAX_ENTITIES_PER_CELL];
    SZ count_per_type[ENTITY_TYPE_COUNT];
    SZ entity_count;
};

struct Grid {
    Vector2 terrain_size;  // Total terrain dimensions
    F32 cell_size;         // Size of each cell (terrain_size.x / GRID_CELLS_PER_ROW)
    GridCell cells[GRID_TOTAL_CELLS];
    SZ cell_index_lookup[GRID_CELLS_PER_ROW][GRID_CELLS_PER_ROW];  // Precomputed indices
};

Grid extern g_grid;

void grid_init(Vector2 terrain_size);
void grid_clear();
void grid_populate();
void grid_add_entity(EID id, EntityType type, Vector3 position);
SZ grid_get_cell_index(Vector3 position);
SZ grid_get_cell_index_xy(S32 grid_x, S32 grid_y);
GridCell *grid_get_cell(Vector3 position);
GridCell *grid_get_cell_by_index(SZ cell_index);
void grid_query_entities_in_radius(Vector3 center, F32 radius, EID *out_entities, SZ *out_count, SZ max_entities);
void grid_query_entities_in_cell(Vector3 position, EID *out_entities, SZ *out_count, SZ max_entities);
void grid_query_entities_around_cell(Vector3 position, EID *out_entities, SZ *out_count, SZ max_entities);
void grid_draw_2d_dbg();
BOOL grid_is_position_valid(Vector3 position);
Vector2 grid_world_to_grid_coords(Vector3 position);
Vector3 grid_cell_center(SZ cell_index);
