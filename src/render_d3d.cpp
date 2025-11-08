#include "assert.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "cvar.hpp"
#include "fog.hpp"
#include "light.hpp"
#include "particles_3d.hpp"
#include "raylib.h"
#include "render.hpp"
#include "std.hpp"
#include "string.hpp"
#include "time.hpp"
#include "world.hpp"

#include <raymath.h>
#include <rlgl.h>
#include <glm/gtc/type_ptr.hpp>

void d3d_cube(Vector3 position, F32 width, F32 height, F32 length, Color color) {
    INCREMENT_DRAW_CALL;

    DrawCube(position, width, height, length, color);
}

void d3d_cube_v(Vector3 position, Vector3 size, Color color) {
    INCREMENT_DRAW_CALL;

    DrawCubeV(position, size, color);
}

void d3d_cube_wires(Vector3 position, F32 width, F32 height, F32 length, Color color) {
    INCREMENT_DRAW_CALL;

    DrawCubeWires(position, width, height, length, color);
}

void d3d_sphere(Vector3 center, F32 radius, Color color) {
    INCREMENT_DRAW_CALL;

    DrawSphere(center, radius, color);
}

void d3d_sphere_ex(Vector3 center, F32 radius, S32 rings, S32 slices, Color color) {
    INCREMENT_DRAW_CALL;

    DrawSphereEx(center, radius, rings, slices, color);
}

void d3d_sphere_wires(Vector3 center, F32 radius, S32 rings, S32 slices, Color color) {
    INCREMENT_DRAW_CALL;

    DrawSphereWires(center, radius, rings, slices, color);
}

void d3d_line(Vector3 start, Vector3 end, Color color) {
    INCREMENT_DRAW_CALL;

    DrawLine3D(start, end, color);
}

void d3d_model_rl(Model *model, Vector3 position, F32 scale, Color tint) {
    INCREMENT_DRAW_CALL;

    S32 enabled = 0;
    SetShaderValue(g_render.model_shader->base, g_render.model_animation_enabled_loc, &enabled, SHADER_UNIFORM_INT);

    DrawModel(*model, position, scale, tint);
}

void d3d_model_ex(AModel *model, Vector3 position, F32 rotation, Vector3 scale, Color tint) {
    INCREMENT_DRAW_CALL;

    S32 enabled = 0;
    SetShaderValue(g_render.model_shader->base, g_render.model_animation_enabled_loc, &enabled, SHADER_UNIFORM_INT);

    // NOTE: Only the Y axis is used for rotation.
    DrawModelEx(model->base, position, (Vector3){0, 1, 0}, rotation, scale, tint);
}

void d3d_model_transform_rl(Model *model, Matrix *transform, Color tint) {
    INCREMENT_DRAW_CALL;

    S32 enabled = 0;
    SetShaderValue(g_render.model_shader->base, g_render.model_animation_enabled_loc, &enabled, SHADER_UNIFORM_INT);

    // Store the material's original diffuse color.
    Color const original_color = model->materials[0].maps[MATERIAL_MAP_DIFFUSE].color;

    // Apply the new tint for this specific draw call.
    model->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = tint;

    // Draw the model's first mesh using the provided transform and the tinted material.
    d3d_mesh_rl(&model->meshes[0], &model->materials[0], transform);

    // Restore the original color to prevent side effects on subsequent draws.
    model->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = original_color;
}

void d3d_model(C8 const *model_name, Vector3 position, F32 rotation, Vector3 scale, Color tint) {
    INCREMENT_DRAW_CALL;

    S32 enabled = 0;
    SetShaderValue(g_render.model_shader->base, g_render.model_animation_enabled_loc, &enabled, SHADER_UNIFORM_INT);

    DrawModelEx(asset_get_model(model_name)->base, position, (Vector3){0, 1, 0}, rotation, scale, tint);
};

void d3d_model_animated(C8 const *model_name, Vector3 position, F32 rotation, Vector3 scale, Color tint, Matrix *bone_matrices, S32 bone_count) {
    INCREMENT_DRAW_CALL;

    S32 enabled = 1;
    SetShaderValue(g_render.model_shader->base, g_render.model_animation_enabled_loc, &enabled, SHADER_UNIFORM_INT);

    AModel *model = asset_get_model(model_name);

    // Build transform matrix
    Matrix mat_scale       = MatrixScale(scale.x, scale.y, scale.z);
    Matrix mat_rotation    = MatrixRotate((Vector3){0, 1, 0}, rotation * DEG2RAD);
    Matrix mat_translation = MatrixTranslate(position.x, position.y, position.z);
    Matrix transform       = MatrixMultiply(MatrixMultiply(mat_scale, mat_rotation), mat_translation);

    // Combine with model's base transform
    transform = MatrixMultiply(model->base.transform, transform);

    // Draw each mesh
    for (S32 i = 0; i < model->base.meshCount; i++) {
        Mesh *mesh         = &model->base.meshes[i];
        Material *material = &model->base.materials[model->base.meshMaterial[i]];

        // Apply tint to material color
        Color original_color = material->maps[MATERIAL_MAP_DIFFUSE].color;
        auto tinted_color    = WHITE;
        tinted_color.r       = (U8)(((S32)original_color.r * (S32)tint.r) / 255);
        tinted_color.g       = (U8)(((S32)original_color.g * (S32)tint.g) / 255);
        tinted_color.b       = (U8)(((S32)original_color.b * (S32)tint.b) / 255);
        tinted_color.a       = (U8)(((S32)original_color.a * (S32)tint.a) / 255);
        material->maps[MATERIAL_MAP_DIFFUSE].color = tinted_color;

        // Temporarily set bone matrices on mesh (if it has bones)
        Matrix *original_bone_matrices = mesh->boneMatrices;
        S32 original_bone_count        = mesh->boneCount;

        if (bone_matrices && mesh->boneIds && mesh->boneWeights) {
            mesh->boneMatrices = bone_matrices;
            mesh->boneCount    = bone_count;
        }

        // Draw the mesh (raylib will upload bone matrices automatically)
        DrawMesh(*mesh, *material, transform);

        // Restore original pointers and color
        mesh->boneMatrices = original_bone_matrices;
        mesh->boneCount    = original_bone_count;
        material->maps[MATERIAL_MAP_DIFFUSE].color = original_color;
    }
}

void d3d_model_instanced(C8 const *model_name, Matrix *transforms, Color *tints, SZ instance_count) {
    INCREMENT_DRAW_CALL;

    AModel *model = asset_get_model(model_name);

    // Set view-projection matrix uniform
    Matrix mat_view_proj = g_render.cameras.c3d.mat_view_proj;
    SetShaderValueMatrix(g_render.model_shader_instanced->base, g_render.model_instanced_mvp_loc, mat_view_proj);

    // Convert colors to float array for GPU
    F32 *instance_colors = (F32 *)RL_MALLOC((S32)instance_count * 4 * sizeof(F32));
    for (SZ j = 0; j < instance_count; ++j) {
        instance_colors[j * 4 + 0] = (F32)tints[j].r / 255.0F;
        instance_colors[j * 4 + 1] = (F32)tints[j].g / 255.0F;
        instance_colors[j * 4 + 2] = (F32)tints[j].b / 255.0F;
        instance_colors[j * 4 + 3] = (F32)tints[j].a / 255.0F;
    }

    // Draw each mesh with instancing
    for (S32 i = 0; i < model->base.meshCount; i++) {
        Mesh *mesh = &model->base.meshes[i];
        Material *material = &model->base.materials[model->base.meshMaterial[i]];

        // Temporarily assign instanced shader to material
        Shader original_material_shader = material->shader;
        material->shader = g_render.model_shader_instanced->base;

        // Use rlgl to draw with custom instance attributes
        rlEnableShader(material->shader.id);

        // Upload transforms (standard instancing)
        rlEnableVertexArray(mesh->vaoId);

        // Set up instance color buffer
        U32 instance_color_buffer = rlLoadVertexBuffer(instance_colors, (S32)instance_count * 4 * sizeof(F32), false);
        S32 color_attrib_loc = rlGetLocationAttrib(material->shader.id, "instanceColor");
        if (color_attrib_loc >= 0) {
            rlEnableVertexBuffer(instance_color_buffer);
            rlSetVertexAttribute((U32)color_attrib_loc, 4, RL_FLOAT, false, 0, 0);
            rlSetVertexAttributeDivisor((U32)color_attrib_loc, 1);  // 1 = per-instance
            rlEnableVertexAttribute((U32)color_attrib_loc);
        }

        // Draw with instancing (using Raylib's built-in transform instancing)
        DrawMeshInstanced(*mesh, *material, transforms, (S32)instance_count);

        // Cleanup
        if (color_attrib_loc >= 0) {
            rlDisableVertexAttribute((U32)color_attrib_loc);
            rlDisableVertexBuffer();
        }
        rlDisableVertexArray();
        rlUnloadVertexBuffer(instance_color_buffer);

        // Restore original shader
        material->shader = original_material_shader;
    }

    RL_FREE(instance_colors);
}

void d3d_mesh_rl(Mesh *mesh, Material *material, Matrix *transform) {
    INCREMENT_DRAW_CALL;

    DrawMesh(*mesh, *material, *transform);
}

void d3d_mesh_rl_instanced(Mesh *mesh, Material *material, Matrix *transforms, SZ instances) {
    INCREMENT_DRAW_CALL;

    DrawMeshInstanced(*mesh, *material, transforms, (S32)instances);
}

void d3d_bounding_box(BoundingBox bb, Color color) {
    INCREMENT_DRAW_CALL;

    DrawBoundingBox(bb, color);
}

void d3d_oriented_bounding_box(OrientedBoundingBox obb, Color color) {
    INCREMENT_DRAW_CALL;

    // Calculate the 8 corners of the OBB
    Vector3 corners[8];
    corners[0] = Vector3Add(obb.center, Vector3{-obb.extents.x, -obb.extents.y, -obb.extents.z});
    corners[1] = Vector3Add(obb.center, Vector3{obb.extents.x, -obb.extents.y, -obb.extents.z});
    corners[2] = Vector3Add(obb.center, Vector3{-obb.extents.x, obb.extents.y, -obb.extents.z});
    corners[3] = Vector3Add(obb.center, Vector3{obb.extents.x, obb.extents.y, -obb.extents.z});
    corners[4] = Vector3Add(obb.center, Vector3{-obb.extents.x, -obb.extents.y, obb.extents.z});
    corners[5] = Vector3Add(obb.center, Vector3{obb.extents.x, -obb.extents.y, obb.extents.z});
    corners[6] = Vector3Add(obb.center, Vector3{-obb.extents.x, obb.extents.y, obb.extents.z});
    corners[7] = Vector3Add(obb.center, Vector3{obb.extents.x, obb.extents.y, obb.extents.z});

    // Rotate each corner
    for (auto &corner : corners) {
        Vector3 rotated;
        Vector3 const point = Vector3Subtract(corner, obb.center);
        rotated.x           = Vector3DotProduct(point, obb.axes[0]);
        rotated.y           = Vector3DotProduct(point, obb.axes[1]);
        rotated.z           = Vector3DotProduct(point, obb.axes[2]);
        corner              = Vector3Add(rotated, obb.center);
    }

    // Draw the edges
    DrawLine3D(corners[0], corners[1], color);  // Bottom face
    DrawLine3D(corners[1], corners[3], color);
    DrawLine3D(corners[3], corners[2], color);
    DrawLine3D(corners[2], corners[0], color);

    DrawLine3D(corners[4], corners[5], color);  // Top face
    DrawLine3D(corners[5], corners[7], color);
    DrawLine3D(corners[7], corners[6], color);
    DrawLine3D(corners[6], corners[4], color);

    DrawLine3D(corners[0], corners[4], color);  // Vertical edges
    DrawLine3D(corners[1], corners[5], color);
    DrawLine3D(corners[2], corners[6], color);
    DrawLine3D(corners[3], corners[7], color);
}

void d3d_terrain(ATerrain *terrain, F32 scale, Color tint) {
    INCREMENT_DRAW_CALL;

    S32 enabled = 0;
    SetShaderValue(g_render.model_shader->base, g_render.model_animation_enabled_loc, &enabled, SHADER_UNIFORM_INT);

    DrawModel(terrain->model, {}, scale, tint);
}

void d3d_terrain_ex(ATerrain *terrain, Vector3 rotation, Vector3 scale, Color tint) {
    INCREMENT_DRAW_CALL;

    S32 enabled = 0;
    SetShaderValue(g_render.model_shader->base, g_render.model_animation_enabled_loc, &enabled, SHADER_UNIFORM_INT);

    F32 const rotationAngle = Vector3Length(rotation);
    DrawModelEx(terrain->model, {}, rotation, rotationAngle, scale, tint);
}

void d3d_plane(Vector3 position, Vector2 size, Color color) {
    INCREMENT_DRAW_CALL;

    DrawPlane(position, size, color);
}

void d3d_skybox(ASkybox *skybox) {
    INCREMENT_DRAW_CALL;

    S32 enabled = 0;
    SetShaderValue(g_render.model_shader->base, g_render.model_animation_enabled_loc, &enabled, SHADER_UNIFORM_INT);

    rlDisableBackfaceCulling();
    rlDisableDepthMask();

    DrawModel(skybox->model, {}, 1.0F, WHITE);

    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void d3d_camera(Camera3D *camera, Color color) {
    INCREMENT_DRAW_CALL;

    Camera3D *active_camera = g_render.cameras.c3d.active_cam;

    if (!c3d_is_point_in_frustum(camera->position)) { return; }

    // Calculate scale factor based on distance
    F32 const dist           = Vector3Distance(g_render.cameras.c3d.active_cam->position, camera->position);
    F32 const frustum_length = glm::max(dist * 0.1F, 0.5F);

    // Get camera vectors
    Vector3 const forward = Vector3Normalize(Vector3Subtract(camera->target, camera->position));
    Vector3 const up      = Vector3Normalize(camera->up);
    Vector3 const right   = Vector3Normalize(Vector3CrossProduct(forward, up));

    // Draw axis indicators
    F32 const axis_length = frustum_length * 1.2F;
    DrawLine3D(camera->position, Vector3Add(camera->position, Vector3Scale(forward, axis_length)), RED);
    DrawLine3D(camera->position, Vector3Add(camera->position, Vector3Scale(up, axis_length)), GREEN);
    DrawLine3D(camera->position, Vector3Add(camera->position, Vector3Scale(right, axis_length)), BLUE);

    // Calculate frustum corners using FOV
    F32 const half_fovy = camera->fovy * 0.5F * DEG2RAD;  // Half of vertical FOV in radians

    // Calculate the exact height at frustum end using tangent
    F32 const far_y = frustum_length * tanf(half_fovy);
    F32 const far_x = far_y * g_render.aspect_ratio;

    // Calculate far plane corners
    Vector3 const frustum_center   = Vector3Add(camera->position, Vector3Scale(forward, frustum_length));
    Vector3 const far_top_left     = Vector3Add(Vector3Add(frustum_center, Vector3Scale(up, far_y)), Vector3Scale(right, -far_x));
    Vector3 const far_top_right    = Vector3Add(Vector3Add(frustum_center, Vector3Scale(up, far_y)), Vector3Scale(right, far_x));
    Vector3 const far_bottom_left  = Vector3Add(Vector3Add(frustum_center, Vector3Scale(up, -far_y)), Vector3Scale(right, -far_x));
    Vector3 const far_bottom_right = Vector3Add(Vector3Add(frustum_center, Vector3Scale(up, -far_y)), Vector3Scale(right, far_x));

    // Lines from camera to far corners
    DrawLine3D(camera->position, far_top_left, color);
    DrawLine3D(camera->position, far_top_right, color);
    DrawLine3D(camera->position, far_bottom_left, color);
    DrawLine3D(camera->position, far_bottom_right, color);

    // Far plane edges
    DrawLine3D(far_top_left, far_top_right, color);
    DrawLine3D(far_top_right, far_bottom_right, color);
    DrawLine3D(far_bottom_right, far_bottom_left, color);
    DrawLine3D(far_bottom_left, far_top_left, color);

    // Draw camera icon
    F32 const size = frustum_length * 0.3F;
    DrawSphere(camera->position, size * 0.75F, ColorAlpha(color, 0.75F));
    Vector3 const icon_position =
        Vector3Add(camera->position, Vector3Scale(Vector3Normalize(Vector3Subtract(active_camera->position, camera->position)), size));
    DrawBillboard(*g_render.cameras.c3d.active_cam, g_render.camera_icon_texture->base, icon_position, size, WHITE);
}

void d3d_billboard(ATexture *texture, Vector3 position, F32 scale, Color tint) {
    INCREMENT_DRAW_CALL;

    DrawBillboard(c3d_get(), texture->base, position, scale, tint);
}

void d3d_collision_mesh(CollisionMesh *collision, Vector3 position, Vector3 scale, Color color) {
    if (!collision || !collision->generated || collision->triangle_count == 0) { return; }

    // Draw wireframe triangles - each triangle is 3 lines
    for (U32 tri_idx = 0; tri_idx < collision->triangle_count; ++tri_idx) {
        U32 const idx0 = collision->indices[(tri_idx * 3) + 0];
        U32 const idx1 = collision->indices[(tri_idx * 3) + 1];
        U32 const idx2 = collision->indices[(tri_idx * 3) + 2];

        // Transform vertices to world space
        Vector3 const v0 = {
            (collision->vertices[(idx0 * 3) + 0] * scale.x) + position.x,
            (collision->vertices[(idx0 * 3) + 1] * scale.y) + position.y,
            (collision->vertices[(idx0 * 3) + 2] * scale.z) + position.z
        };
        Vector3 const v1 = {
            (collision->vertices[(idx1 * 3) + 0] * scale.x) + position.x,
            (collision->vertices[(idx1 * 3) + 1] * scale.y) + position.y,
            (collision->vertices[(idx1 * 3) + 2] * scale.z) + position.z
        };
        Vector3 const v2 = {
            (collision->vertices[(idx2 * 3) + 0] * scale.x) + position.x,
            (collision->vertices[(idx2 * 3) + 1] * scale.y) + position.y,
            (collision->vertices[(idx2 * 3) + 2] * scale.z) + position.z
        };

        // Draw the 3 edges of the triangle
        INCREMENT_DRAW_CALL;
        DrawLine3D(v0, v1, color);
        DrawLine3D(v1, v2, color);
        DrawLine3D(v2, v0, color);
    }
}

void d3d_gizmo(Vector3 position, F32 rotation, OrientedBoundingBox bbox, Color bbox_color, F32 gizmos_alpha, BOOL has_talker, BOOL is_selected) {
    INCREMENT_DRAW_CALL;

    if (c_debug__bbox_info || is_selected) { d3d_oriented_bounding_box(bbox, Fade(bbox_color, gizmos_alpha)); }

    if (has_talker && Vector3Distance(position, g_render.cameras.c3d.default_cam.position) > 1.0F) {
        Vector3 const talker_icon_pos = {position.x + 0.1F, position.y, position.z};
        DrawBillboard(g_render.cameras.c3d.default_cam, g_render.speaker_icon_texture->base, talker_icon_pos, 0.1F, Fade(WHITE, gizmos_alpha));
    }

    if (is_selected) {
        // Calculate base scale using logarithmic scaling for large objects
        F32 const bbox_max_size = glm::max(bbox.extents.x, glm::max(bbox.extents.y, bbox.extents.z)) * 2.0F;
        F32 base_scale = bbox_max_size;
        if (bbox_max_size > 10.0F) {
            // Apply logarithmic scaling for large objects
            base_scale = 10.0F + (std::log10(bbox_max_size / 10.0F) * 5.0F);
        }
        base_scale = glm::clamp(base_scale, 0.1F, 1000.0F);

        // Use square root scaling for radii to prevent excessive thickness
        F32 const sqrt_base_scale = math_sqrt_f32(base_scale);

        // Adjust proportions for visual elements to prevent them from becoming too thick
        F32 const arrow_length      = base_scale / 1.5F;
        F32 const arrow_head_length = glm::min(base_scale * 0.2F, 5.0F);  // Cap head length

        // Use square root scaling for radii to prevent excessive thickness
        F32 const arrow_radius      = glm::min(sqrt_base_scale * 0.02F, 1.0F);
        F32 const arrow_head_radius = glm::min(sqrt_base_scale * 0.06F, 2.0F);

        // Scale cubes and spheres more conservatively
        F32 const scale_cube_size      = glm::min(sqrt_base_scale * 0.08F, 2.0F);
        F32 const center_sphere_radius = glm::min(sqrt_base_scale * 0.06F, 2.0F);

        // Scale rotation ring more conservatively
        F32 const rotation_radius = glm::min(base_scale * 0.6F, 100.0F);
        S32 const circle_segments = 32;

        // Create rotation matrix for Y rotation only
        Matrix const rotation_matrix = MatrixRotateY(rotation * DEG2RAD);

        // Center sphere
        DrawSphere(position, center_sphere_radius, BLACK);

        // Movement arrows (rotated)
        Vector3 const dirs[] = {{arrow_length, 0, 0}, {0, arrow_length, 0}, {0, 0, arrow_length}};
        Color const colors[] = {RED, GREEN, BLUE};
        for (SZ i = 0; i < 3; i++) {
            Vector3 const dir            = Vector3Transform(dirs[i], rotation_matrix);
            Vector3 const arrow_end      = Vector3Add(position, dir);
            Vector3 const head_dir       = Vector3Scale(dir, (arrow_length + arrow_head_length) / arrow_length);
            Vector3 const arrow_head_end = Vector3Add(position, head_dir);
            DrawCylinderEx(position, arrow_end, arrow_radius, arrow_radius, 8, colors[i]);
            DrawCylinderEx(arrow_end, arrow_head_end, arrow_head_radius, 0, 8, colors[i]);
        }

        // Y rotation ring
        for (SZ i = 0; i < circle_segments; i++) {
            F32 const angle      = (2.0F * glm::pi<F32>() * (F32)i) / circle_segments;
            F32 const next_angle = (2.0F * glm::pi<F32>() * (F32)(i + 1)) / circle_segments;
            Vector3 const start = {
                position.x + (rotation_radius * math_cos_f32(angle)),
                position.y,
                position.z + (rotation_radius * math_sin_f32(angle)),
            };
            Vector3 const end = {
                position.x + (rotation_radius * math_cos_f32(next_angle)),
                position.y,
                position.z + (rotation_radius * math_sin_f32(next_angle)),
            };
            DrawLine3D(start, end, YELLOW);
        }

        // Scale handles (rotated)
        Vector3 scale_positions[] = {{arrow_length * 1.2F, 0, 0}, {0, arrow_length * 1.2F, 0}, {0, 0, arrow_length * 1.2F}};
        for (auto &scale_position : scale_positions) {
            scale_position = Vector3Transform(scale_position, rotation_matrix);
            scale_position = Vector3Add(position, scale_position);
            DrawCube(scale_position, scale_cube_size, scale_cube_size, scale_cube_size, MAGENTA);
        }
    }
}

void d3d_bone_gizmo(EID entity_id) {
    if (!g_world->animation[entity_id].has_animations) { return; }

    AModel *model = asset_get_model(g_world->model_name[entity_id]);
    if (!model || !model->animations) { return; }

    U32 const anim_idx = g_world->animation[entity_id].anim_index;
    U32 const frame    = g_world->animation[entity_id].anim_frame;

    if (anim_idx >= (U32)model->animation_count) { return; }

    ModelAnimation const& anim = model->animations[anim_idx];
    if (frame >= (U32)anim.frameCount) { return; }

    S32 const bone_count = g_world->animation[entity_id].bone_count;

    // Build entity transform matrix (same as d3d_model_animated)
    Vector3 const position = g_world->position[entity_id];
    Vector3 const scale    = g_world->scale[entity_id];
    F32 const rotation     = g_world->rotation[entity_id];

    Matrix mat_scale    = MatrixScale(scale.x, scale.y, scale.z);
    Matrix mat_rotation = MatrixRotate((Vector3){0, 1, 0}, rotation * DEG2RAD);
    Matrix mat_position = MatrixTranslate(position.x, position.y, position.z);
    Matrix entity_transform = MatrixMultiply(MatrixMultiply(mat_scale, mat_rotation), mat_position);

    // Compute world-space bone matrices by accumulating through hierarchy
    Matrix bone_world_matrices[ENTITY_MAX_BONES];
    for (S32 bone_idx = 0; bone_idx < bone_count; bone_idx++) {
        Transform *bone_transform = &anim.framePoses[frame][bone_idx];

        // Build local bone matrix
        Matrix local_matrix = MatrixMultiply(MatrixMultiply(
            MatrixScale(bone_transform->scale.x, bone_transform->scale.y, bone_transform->scale.z),
            QuaternionToMatrix(bone_transform->rotation)),
            MatrixTranslate(bone_transform->translation.x, bone_transform->translation.y, bone_transform->translation.z));

        // Accumulate parent transforms (parent * local for proper hierarchy)
        S32 parent_idx = model->base.bones[bone_idx].parent;
        if (parent_idx >= 0) {
            bone_world_matrices[bone_idx] = MatrixMultiply(bone_world_matrices[parent_idx], local_matrix);
        } else {
            bone_world_matrices[bone_idx] = local_matrix;
        }
    }

    // Draw all bones as spheres
    for (S32 bone_idx = 0; bone_idx < bone_count; bone_idx++) {
        // Transform bone position to world space using entity transform
        Matrix final_bone_matrix = MatrixMultiply(bone_world_matrices[bone_idx], entity_transform);
        Vector3 world_pos = {final_bone_matrix.m12, final_bone_matrix.m13, final_bone_matrix.m14};

        // Scale bone visualization with entity scale (use average scale)
        F32 const avg_scale   = (scale.x + scale.y + scale.z) / 3.0F;
        F32 const bone_radius = 0.05F * avg_scale;
        F32 const wire_radius = 0.08F * avg_scale;

        // Draw bone sphere (solid)
        Color bone_color = Fade(YELLOW, 0.6F);
        d3d_sphere(world_pos, bone_radius, bone_color);

        // Draw wireframe sphere for depth perception
        d3d_sphere_wires(world_pos, wire_radius, 8, 8, YELLOW);
    }

    INCREMENT_DRAW_CALL;
}
