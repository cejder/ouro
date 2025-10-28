#include "grid.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "cvar.hpp"
#include "log.hpp"
#include "math.hpp"
#include "render.hpp"
#include "std.hpp"
#include "world.hpp"

#include <raymath.h>

Grid g_grid = {};

void grid_init(Vector2 terrain_size) {
    g_grid.terrain_size = terrain_size;
    g_grid.cell_size    = terrain_size.x / (F32)GRID_CELLS_PER_ROW;
    for (auto &cell : g_grid.cells) { cell.entity_count = 0; }

    // Precompute cell index lookup table
    for (S32 y = 0; y < GRID_CELLS_PER_ROW; ++y) {
        for (S32 x = 0; x < GRID_CELLS_PER_ROW; ++x) { g_grid.cell_index_lookup[y][x] = (((SZ)y * GRID_CELLS_PER_ROW) + (SZ)x); }
    }

    llt("Grid initialized: %dx%d cells, cell_size: %.2f", GRID_CELLS_PER_ROW, GRID_CELLS_PER_ROW, g_grid.cell_size);
}

void grid_clear() {
    ou_memset(g_grid.cells, 0, sizeof(g_grid.cells));
}

void grid_populate() {
    grid_clear();

    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const i = g_world->active_entities[idx];
        grid_add_entity(i, g_world->type[i], g_world->position[i]);
    }
}

void grid_add_entity(EID id, EntityType type, Vector3 position) {
    if (!grid_is_position_valid(position)) { return; }

    SZ const cell_index = grid_get_cell_index(position);
    GridCell *cell      = &g_grid.cells[cell_index];

    if (cell->entity_count >= GRID_MAX_ENTITIES_PER_CELL) {
        llt("Grid cell %zu is full, cannot add entity %u", cell_index, id);
        return;
    }

    cell->entities[cell->entity_count] = id;
    cell->count_per_type[type]++;
    cell->entity_count++;
}

SZ grid_get_cell_index(Vector3 position) {
    Vector2 const grid_coords = grid_world_to_grid_coords(position);
    return grid_get_cell_index_xy((S32)grid_coords.x, (S32)grid_coords.y);
}

SZ grid_get_cell_index_xy(S32 grid_x, S32 grid_y) {
    _assert_(grid_x >= 0, "grid_x negative");
    _assert_(grid_x < GRID_CELLS_PER_ROW, "grid_x too large");
    _assert_(grid_y >= 0, "grid_y negative");
    _assert_(grid_y < GRID_CELLS_PER_ROW, "grid_y too large");
    return g_grid.cell_index_lookup[grid_y][grid_x];
}

GridCell *grid_get_cell(Vector3 position) {
    return &g_grid.cells[grid_get_cell_index(position)];
}

GridCell *grid_get_cell_by_index(SZ cell_index) {
    if (cell_index >= (SZ)GRID_TOTAL_CELLS) { return nullptr; }
    return &g_grid.cells[cell_index];
}

void grid_query_entities_in_radius(Vector3 center, F32 radius, EID *out_entities, SZ *out_count, SZ max_entities) {
    *out_count = 0;

    // Calculate which cells the radius might overlap
    Vector2 const grid_center = grid_world_to_grid_coords(center);
    F32 const radius_in_cells = radius / g_grid.cell_size;
    F32 const radius_sqr      = radius * radius;

    S32 min_x = (S32)(grid_center.x - radius_in_cells - 1);
    S32 max_x = (S32)(grid_center.x + radius_in_cells + 1);
    S32 min_y = (S32)(grid_center.y - radius_in_cells - 1);
    S32 max_y = (S32)(grid_center.y + radius_in_cells + 1);

    // Clamp to grid bounds
    min_x = glm::max(min_x, 0);
    max_x = glm::min(max_x, GRID_CELLS_PER_ROW - 1);
    min_y = glm::max(min_y, 0);
    max_y = glm::min(max_y, GRID_CELLS_PER_ROW - 1);

    // Check entities in relevant cells
    for (S32 y = min_y; y <= max_y; ++y) {
        for (S32 x = min_x; x <= max_x; ++x) {
            GridCell *cell = &g_grid.cells[grid_get_cell_index_xy(x, y)];

            for (SZ i = 0; i < cell->entity_count; ++i) {
                EID const entity_id      = cell->entities[i];
                Vector3 const entity_pos = g_world->position[entity_id];

                // Check actual distance
                F32 const distance_sqr = Vector3DistanceSqr(center, entity_pos);
                if (distance_sqr <= radius_sqr && *out_count < max_entities) {
                    out_entities[*out_count] = entity_id;
                    (*out_count)++;
                }
            }
        }
    }
}

void grid_query_entities_in_cell(Vector3 position, EID *out_entities, SZ *out_count, SZ max_entities) {
    *out_count = 0;

    GridCell *cell      = grid_get_cell(position);
    SZ const copy_count = glm::min(cell->entity_count, max_entities);
    for (SZ i = 0; i < copy_count; ++i) { out_entities[i] = cell->entities[i]; }
    *out_count = copy_count;
}

void grid_query_entities_around_cell(Vector3 position, EID *out_entities, SZ *out_count, SZ max_entities) {
    *out_count = 0;

    Vector2 const grid_coords = grid_world_to_grid_coords(position);
    S32 const center_x        = (S32)grid_coords.x;
    S32 const center_y        = (S32)grid_coords.y;

    // Check 3x3 area around the cell
    for (S32 dy = -1; dy <= 1; ++dy) {
        for (S32 dx = -1; dx <= 1; ++dx) {
            S32 const x = center_x + dx;
            S32 const y = center_y + dy;

            // Skip if outside grid bounds
            if (x < 0 || x >= GRID_CELLS_PER_ROW || y < 0 || y >= GRID_CELLS_PER_ROW) { continue; }

            GridCell *cell = &g_grid.cells[grid_get_cell_index_xy(x, y)];

            for (SZ i = 0; i < cell->entity_count && *out_count < max_entities; ++i) {
                out_entities[*out_count] = cell->entities[i];
                (*out_count)++;
            }
        }
    }
}

void grid_draw_2d_dbg() {
    if (!c_debug__grid_info) { return; }

    Vector2 const render_res = {(F32)c_video__render_resolution_width, (F32)c_video__render_resolution_height};

    // Map dimensions
    F32 const map_height   = ui_scale_y(50.0F);
    F32 const map_width    = map_height * (g_grid.terrain_size.x / g_grid.terrain_size.y);
    Vector2 const map_pos  = {ui_scale_x(1.25F), ((render_res.y - ui_scale_y(1.25F)) - map_height)};
    Vector2 const map_size = {map_width, map_height};

    // Draw terrain texture as background
    Rectangle const dst_rec = {
        map_pos.x,
        map_pos.y,
        map_size.x,
        map_size.y,
    };
    Rectangle const src_rec = {
        0,
        0,
        (F32)g_world->base_terrain->diffuse_texture->base.width,
        (F32)g_world->base_terrain->diffuse_texture->base.height,
    };
    d2d_texture_pro(g_world->base_terrain->diffuse_texture, src_rec, dst_rec, {0, 0}, 0.0F, Fade(WHITE, 0.85F));
    d2d_rectangle_rec({map_pos.x, map_pos.y, map_size.x, map_size.y}, Fade(BLACK, 0.4F));
    d2d_rectangle_rec_lines({map_pos.x - 1, map_pos.y - 1, map_size.x + 2, map_size.y + 2}, Fade(NAYBEIGE, 0.85F));

    // Draw entity OBBs on top of heatmap - brighter colors for transparency
    Color static const i_entity_type_debug_colors[ENTITY_TYPE_COUNT] = {
        GRAY,    // NONE
        YELLOW,  // NPC
        GREEN,   // VEGETATION
        ORANGE,  // BUILDING_LUMBERYARD
        CYAN,    // PROP
    };

    for (SZ idx = 0; idx < g_world->active_entity_count; ++idx) {
        EID const entity_id           = g_world->active_entities[idx];
        Vector3 const position        = g_world->position[entity_id];
        EntityType const type         = g_world->type[entity_id];
        OrientedBoundingBox const obb = g_world->obb[entity_id];

        // Convert world position to screen coordinates (match camera mapping)
        Vector2 const screen_pos = {
            map_pos.x + ((position.x / g_grid.terrain_size.x) * map_size.x),
            map_pos.y + ((position.z / g_grid.terrain_size.y) * map_size.y),
        };

        // Convert OBB extents to screen scale
        F32 const scale_x = (obb.extents.x * 2.0F / g_grid.terrain_size.x) * map_size.x;
        F32 const scale_y = (obb.extents.z * 2.0F / g_grid.terrain_size.y) * map_size.y;

        // Draw OBB as wireframe rectangle
        Rectangle const obb_rec = {
            screen_pos.x - (scale_x * 0.5F),
            screen_pos.y - (scale_y * 0.5F),
            scale_x,
            scale_y,
        };

        d2d_rectangle_rec_lines(obb_rec, i_entity_type_debug_colors[type]);
    }

    // Camera frustum visualization
    Camera3D const camera    = c3d_get();
    Vector3 const cam_pos    = camera.position;
    Vector3 const cam_target = camera.target;
    F32 const fov_rad        = c3d_get_fov() * DEG2RAD;
    F32 const aspect         = g_render.aspect_ratio;

    // Convert camera position to screen
    Vector2 const cam_screen_pos = {
        map_pos.x + ((cam_pos.x / g_grid.terrain_size.x) * map_size.x),
        map_pos.y + ((cam_pos.z / g_grid.terrain_size.y) * map_size.y),
    };

    // Calculate frustum projection
    Vector3 const forward = Vector3Normalize(Vector3Subtract(cam_target, cam_pos));
    Vector3 const right   = Vector3Normalize(Vector3CrossProduct(forward, {0, 1, 0}));

    F32 const frustum_distance = 80.0F;  // How far to project frustum lines
    F32 const half_width       = frustum_distance * tanf(fov_rad * 0.5F) * aspect;

    // Calculate frustum corners in world space (simplified to 2D)
    Vector3 const frustum_center = Vector3Add(cam_pos, Vector3Scale(forward, frustum_distance));
    Vector3 const frustum_corners[2] = {
        Vector3Add(frustum_center, Vector3Scale(right, -half_width)),
        Vector3Add(frustum_center, Vector3Scale(right, half_width)),
    };

    // Convert frustum corners to screen coordinates
    Vector2 frustum_screen[2];
    for (S32 j = 0; j < 2; ++j) {
        frustum_screen[j] = {
            map_pos.x + ((frustum_corners[j].x / g_grid.terrain_size.x) * map_size.x),
            map_pos.y + ((frustum_corners[j].z / g_grid.terrain_size.y) * map_size.y),
        };
    }

    // Draw frustum outline
    Color const frustum_line_color = Fade(MAGENTA, 0.7F);
    d2d_line(cam_screen_pos, frustum_screen[0], frustum_line_color);
    d2d_line(cam_screen_pos, frustum_screen[1], frustum_line_color);
    d2d_line(frustum_screen[0], frustum_screen[1], frustum_line_color);

    // Draw camera position with direction indicator
    d2d_circle(cam_screen_pos, 4.0F, RED);
    d2d_circle(cam_screen_pos, 2.0F, WHITE);

    // Camera direction line
    Vector2 const cam_forward_screen = {
        cam_screen_pos.x + ((forward.x / g_grid.terrain_size.x) * map_size.x * 0.1F),
        cam_screen_pos.y + ((forward.z / g_grid.terrain_size.y) * map_size.y * 0.1F),
    };
    d2d_line(cam_screen_pos, cam_forward_screen, WHITE);
}

BOOL grid_is_position_valid(Vector3 position) {
    return position.x >= 0.0F && position.x < g_grid.terrain_size.x && position.z >= 0.0F && position.z < g_grid.terrain_size.y;
}

Vector2 grid_world_to_grid_coords(Vector3 position) {
    return {position.x / g_grid.cell_size, position.z / g_grid.cell_size};
}

Vector3 grid_cell_center(SZ cell_index) {
    S32 const x = (S32)(cell_index % GRID_CELLS_PER_ROW);
    S32 const y = (S32)(cell_index / GRID_CELLS_PER_ROW);

    return {((F32)x + 0.5F) * g_grid.cell_size, 0.0F, ((F32)y + 0.5F) * g_grid.cell_size};
}
