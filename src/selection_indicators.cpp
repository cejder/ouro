#include "selection_indicators.hpp"
#include "asset.hpp"
#include "math.hpp"
#include "render.hpp"
#include "scene.hpp"

#include <raymath.h>
#include <rlgl.h>
#include <external/glad.h>

SelectionIndicators g_selection_indicators = {};

void selection_indicators_init() {
    lli("Initializing selection indicators system...");

    // Load shader
    g_selection_indicators.shader = asset_get_shader("selection_indicators");
    if (!g_selection_indicators.shader) {
        lle("Failed to load selection_indicators shader!");
        return;
    }
    lli("Selection indicators shader loaded successfully");

    // Cache uniform locations
    g_selection_indicators.view_proj_loc = GetShaderLocation(g_selection_indicators.shader->base, "u_view_proj");
    g_selection_indicators.camera_pos_loc = GetShaderLocation(g_selection_indicators.shader->base, "u_camera_pos");
    g_selection_indicators.camera_right_loc = GetShaderLocation(g_selection_indicators.shader->base, "u_camera_right");
    g_selection_indicators.camera_up_loc = GetShaderLocation(g_selection_indicators.shader->base, "u_camera_up");

    // Set indicator color - nice RTS green with some alpha
    g_selection_indicators.indicator_color = {0.2F, 1.0F, 0.3F, 0.8F};

    // Base quad vertices for instanced rendering (billboards)
    F32 const quad_vertices[] = {
        // Positions  // TexCoords
        -0.5F,  0.5F,   0.0F, 1.0F,   // Top-left
         0.5F, -0.5F,   1.0F, 0.0F,   // Bottom-right
        -0.5F, -0.5F,   0.0F, 0.0F,   // Bottom-left
        -0.5F,  0.5F,   0.0F, 1.0F,   // Top-left
         0.5F,  0.5F,   1.0F, 1.0F,   // Top-right
         0.5F, -0.5F,   1.0F, 0.0F    // Bottom-right
    };

    // Create VAO and quad VBO
    glGenVertexArrays(1, &g_selection_indicators.vao);
    glGenBuffers(1, &g_selection_indicators.quad_vbo);
    glGenBuffers(1, &g_selection_indicators.vbo);

    glBindVertexArray(g_selection_indicators.vao);

    // Setup quad VBO
    glBindBuffer(GL_ARRAY_BUFFER, g_selection_indicators.quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Vertex attributes (location 0 = position, location 1 = texcoord)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(F32), (void*)nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(F32), (void*)(2 * sizeof(F32)));

    // Create instance data SSBO
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_selection_indicators.vbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 SELECTION_INDICATOR_MAX_COUNT * sizeof(SelectionIndicatorInstanceData),
                 nullptr, GL_DYNAMIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Initialize indicator array
    for (SZ i = 0; i < SELECTION_INDICATOR_MAX_COUNT; ++i) {
        g_selection_indicators.indicators[i].entity_id = {};
        g_selection_indicators.indicators[i].rotation = 0.0F;
        g_selection_indicators.indicators[i].active = false;
    }
    g_selection_indicators.active_count = 0;

    lli("Selection indicators initialized successfully");
}

void selection_indicators_shutdown() {
    if (g_selection_indicators.vao != 0) {
        glDeleteVertexArrays(1, &g_selection_indicators.vao);
        g_selection_indicators.vao = 0;
    }
    if (g_selection_indicators.quad_vbo != 0) {
        glDeleteBuffers(1, &g_selection_indicators.quad_vbo);
        g_selection_indicators.quad_vbo = 0;
    }
    if (g_selection_indicators.vbo != 0) {
        glDeleteBuffers(1, &g_selection_indicators.vbo);
        g_selection_indicators.vbo = 0;
    }
}

void selection_indicators_update(F32 dt) {
    // Update rotations for all active indicators
    for (SZ i = 0; i < SELECTION_INDICATOR_MAX_COUNT; ++i) {
        if (g_selection_indicators.indicators[i].active) {
            g_selection_indicators.indicators[i].rotation += SELECTION_INDICATOR_ROTATION_SPEED * dt;

            // Keep rotation in [0, 2PI] range
            if (g_selection_indicators.indicators[i].rotation > 6.28318530718F) {
                g_selection_indicators.indicators[i].rotation -= 6.28318530718F;
            }
        }
    }
}

void selection_indicators_draw() {
    if (!g_selection_indicators.shader) {
        return;
    }

    if (g_selection_indicators.active_count == 0) {
        return;
    }

    // Build instance data from active indicators
    SelectionIndicatorInstanceData instance_data[SELECTION_INDICATOR_MAX_COUNT];
    SZ instance_count = 0;

    for (SZ i = 0; i < SELECTION_INDICATOR_MAX_COUNT; ++i) {
        if (!g_selection_indicators.indicators[i].active) {
            continue;
        }

        EID id = g_selection_indicators.indicators[i].entity_id;
        if (!entity_is_valid(id)) {
            // Entity no longer valid, deactivate indicator
            g_selection_indicators.indicators[i].active = false;
            g_selection_indicators.active_count--;
            continue;
        }

        // Get entity position and radius
        Vector3 position = g_world->position[id];
        F32 radius = g_world->radius[id];

        // Calculate terrain normal and adjust Y position
        Vector3 terrain_normal = math_get_terrain_normal(g_world->base_terrain, position.x, position.z);
        F32 y_offset = SELECTION_INDICATOR_Y_OFFSET * (1.0F - terrain_normal.y);
        position.y += y_offset;

        // Fill instance data
        instance_data[instance_count].position = position;
        instance_data[instance_count].rotation = g_selection_indicators.indicators[i].rotation;
        instance_data[instance_count].size = radius * 2.0F;  // Same sizing as old particle system
        instance_data[instance_count].terrain_normal_x = terrain_normal.x;
        instance_data[instance_count].terrain_normal_y = terrain_normal.y;
        instance_data[instance_count].terrain_normal_z = terrain_normal.z;
        instance_data[instance_count].color = g_selection_indicators.indicator_color;
        instance_data[instance_count].padding[0] = 0.0F;
        instance_data[instance_count].padding[1] = 0.0F;

        instance_count++;
    }

    if (instance_count == 0) {
        return;
    }

    // Upload instance data to GPU
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_selection_indicators.vbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    instance_count * sizeof(SelectionIndicatorInstanceData),
                    instance_data);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, g_selection_indicators.vbo);

    // Set OpenGL state for transparent rendering (match particle system)
    Camera3D* camera = g_render.cameras.c3d.active_cam;
    glDisable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);

    BeginShaderMode(g_selection_indicators.shader->base);

    // Get matrices from Raylib's matrix stack (same as particles)
    Matrix const view_matrix = rlGetMatrixModelview();
    Matrix const proj_matrix = rlGetMatrixProjection();
    Matrix const view_proj   = MatrixMultiply(view_matrix, proj_matrix);
    SetShaderValueMatrix(g_selection_indicators.shader->base, g_selection_indicators.view_proj_loc, view_proj);

    // Calculate camera right and up vectors for billboarding (same as particles)
    Vector3 const forward = Vector3Normalize(Vector3Subtract(camera->target, camera->position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera->up));
    Vector3 up = Vector3CrossProduct(right, forward);

    SetShaderValue(g_selection_indicators.shader->base, g_selection_indicators.camera_pos_loc, &camera->position, SHADER_UNIFORM_VEC3);
    SetShaderValue(g_selection_indicators.shader->base, g_selection_indicators.camera_right_loc, &right, SHADER_UNIFORM_VEC3);
    SetShaderValue(g_selection_indicators.shader->base, g_selection_indicators.camera_up_loc, &up, SHADER_UNIFORM_VEC3);

    // Draw instanced
    glBindVertexArray(g_selection_indicators.vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)instance_count);
    glBindVertexArray(0);

    EndShaderMode();

    // Restore OpenGL state
    glEnable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    INCREMENT_DRAW_CALL;
}

void selection_indicators_clear() {
    for (SZ i = 0; i < SELECTION_INDICATOR_MAX_COUNT; ++i) {
        g_selection_indicators.indicators[i].active = false;
        g_selection_indicators.indicators[i].rotation = 0.0F;
    }
    g_selection_indicators.active_count = 0;
}

void selection_indicators_add(EID entity_id) {
    if (!entity_is_valid(entity_id)) {
        return;
    }

    // Check if indicator already exists for this entity
    for (SZ i = 0; i < SELECTION_INDICATOR_MAX_COUNT; ++i) {
        if (g_selection_indicators.indicators[i].active &&
            g_selection_indicators.indicators[i].entity_id == entity_id) {
            return; // Already has an indicator
        }
    }

    // Find first inactive slot
    for (SZ i = 0; i < SELECTION_INDICATOR_MAX_COUNT; ++i) {
        if (!g_selection_indicators.indicators[i].active) {
            g_selection_indicators.indicators[i].entity_id = entity_id;
            g_selection_indicators.indicators[i].rotation = 0.0F;
            g_selection_indicators.indicators[i].active = true;
            g_selection_indicators.active_count++;
            return;
        }
    }
}

void selection_indicators_remove(EID entity_id) {
    for (SZ i = 0; i < SELECTION_INDICATOR_MAX_COUNT; ++i) {
        if (g_selection_indicators.indicators[i].active &&
            g_selection_indicators.indicators[i].entity_id == entity_id) {
            g_selection_indicators.indicators[i].active = false;
            g_selection_indicators.active_count--;
            return;
        }
    }
}

void selection_indicators_sync_with_world() {
    // Clear all indicators first
    selection_indicators_clear();

    // Add indicators for all selected entities
    for (SZ i = 0; i < g_world->selected_entity_count; ++i) {
        selection_indicators_add(g_world->selected_entities[i]);
    }
}
