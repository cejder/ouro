#include "message.hpp"
#include "render.hpp"
#include "world.hpp"

#include <raymath.h>

void c3d_set(Camera3D *camera) {
    g_render.cameras.c3d.active_cam = camera;
}

Camera3D *c3d_get_ptr() {
    return g_render.cameras.c3d.active_cam;
}

Camera3D c3d_get() {
    return *g_render.cameras.c3d.active_cam;
}

Camera3D *c3d_get_default_ptr() {
    return &g_render.cameras.c3d.default_cam;
}

Camera3D c3d_get_default() {
    return g_render.cameras.c3d.default_cam;
}

void c3d_set_position(Vector3 position) {
    g_render.cameras.c3d.active_cam->position = position;
}

void c3d_set_target(Vector3 target) {
    g_render.cameras.c3d.active_cam->target = target;
}

void c3d_set_up(Vector3 up) {
    g_render.cameras.c3d.active_cam->up = up;
}

F32 c3d_get_fov() {
    return g_render.cameras.c3d.active_cam->fovy;
}

void c3d_set_fov(F32 fov) {
    g_render.cameras.c3d.active_cam->fovy = glm::clamp(fov, CAMERA3D_MIN_FOV, CAMERA3D_MAX_FOV);
}

void c3d_adjust_fov(F32 delta) {
    c3d_set_fov(g_render.cameras.c3d.active_cam->fovy + delta);
}

void c3d_reset() {
    g_render.cameras.c3d.active_cam             = &g_render.cameras.c3d.default_cam;
    g_render.cameras.c3d.active_cam->position   = {};
    g_render.cameras.c3d.active_cam->target     = {};
    g_render.cameras.c3d.active_cam->up         = {0.0F, 1.0F, 0.0F};
    g_render.cameras.c3d.active_cam->fovy       = CAMERA3D_DEFAULT_FOV;
    g_render.cameras.c3d.active_cam->projection = CAMERA_PERSPECTIVE;
}

void c3d_cb_reset(void *data) {
    auto *rd = (C3DResetData *)data;

    Camera3D *other   = rd->other;
    other->position   = rd->other_positon;
    other->target     = rd->other_target;
    other->up         = rd->other_up;
    other->fovy       = rd->other_fovy;
    other->projection = rd->other_projection;

    g_render.cameras.c3d.default_cam.position   = rd->default_positon;
    g_render.cameras.c3d.default_cam.target     = rd->default_target;
    g_render.cameras.c3d.default_cam.up         = rd->default_up;
    g_render.cameras.c3d.default_cam.fovy       = rd->default_fovy;
    g_render.cameras.c3d.default_cam.projection = rd->default_projection;

    if (!rd->notify) { mi("Cameras reset", WHITE); }
}

void c3d_cb_toggle(void *data) {
    auto *cam = (Camera3D *)data;
    Camera3D const *active_cam = c3d_get_ptr();

    if (active_cam == cam) {
        c3d_set(c3d_get_default_ptr());
        mio("Switching to \\ouc{#ffff00ff}default camera", WHITE);
        g_player.non_player_camera = true;
    } else {
        c3d_set(cam);
        mio("Switching to \\ouc{#00ff00ff}player camera", WHITE);
        g_player.non_player_camera = false;
    }
}

void c3d_copy_from_other(Camera3D *dst, Camera3D *src) {
    dst->position   = src->position;
    dst->target     = src->target;
    dst->up         = src->up;
    dst->fovy       = src->fovy;
    dst->projection = src->projection;
}

void c3d_pull_default_to_other(Camera3D *src) {
    mio("Pulling \\ouc{#ffff00ff}default camera \\ouc{#ffffffff}to \\ouc{#00ff00ff}other camera", WHITE);
    Camera3D *default_camera = c3d_get_default_ptr();
    default_camera->position = src->position;
    default_camera->target   = src->target;
}

void static i_calculate_frustum_planes(Vector4 frustum_planes[RENDER_FRUSTUM_PLANE_COUNT], BOOL is_ortho) {
    // Extract frustum planes using Gribb-Hartmann method
    // Left plane
    frustum_planes[0] = Vector4{
        g_render.cameras.c3d.mat_view_proj.m3 + g_render.cameras.c3d.mat_view_proj.m0,
        g_render.cameras.c3d.mat_view_proj.m7 + g_render.cameras.c3d.mat_view_proj.m4,
        g_render.cameras.c3d.mat_view_proj.m11 + g_render.cameras.c3d.mat_view_proj.m8,
        g_render.cameras.c3d.mat_view_proj.m15 + g_render.cameras.c3d.mat_view_proj.m12,
    };
    // Right plane
    frustum_planes[1] = Vector4{
        g_render.cameras.c3d.mat_view_proj.m3 - g_render.cameras.c3d.mat_view_proj.m0,
        g_render.cameras.c3d.mat_view_proj.m7 - g_render.cameras.c3d.mat_view_proj.m4,
        g_render.cameras.c3d.mat_view_proj.m11 - g_render.cameras.c3d.mat_view_proj.m8,
        g_render.cameras.c3d.mat_view_proj.m15 - g_render.cameras.c3d.mat_view_proj.m12,
    };
    // Top plane
    frustum_planes[2] = Vector4{
        g_render.cameras.c3d.mat_view_proj.m3 - g_render.cameras.c3d.mat_view_proj.m1,
        g_render.cameras.c3d.mat_view_proj.m7 - g_render.cameras.c3d.mat_view_proj.m5,
        g_render.cameras.c3d.mat_view_proj.m11 - g_render.cameras.c3d.mat_view_proj.m9,
        g_render.cameras.c3d.mat_view_proj.m15 - g_render.cameras.c3d.mat_view_proj.m13,
    };
    // Bottom plane
    frustum_planes[3] = Vector4{
        g_render.cameras.c3d.mat_view_proj.m3 + g_render.cameras.c3d.mat_view_proj.m1,
        g_render.cameras.c3d.mat_view_proj.m7 + g_render.cameras.c3d.mat_view_proj.m5,
        g_render.cameras.c3d.mat_view_proj.m11 + g_render.cameras.c3d.mat_view_proj.m9,
        g_render.cameras.c3d.mat_view_proj.m15 + g_render.cameras.c3d.mat_view_proj.m13,
    };

    // Near and far planes differ between perspective and orthographic
    if (is_ortho) {
        // For orthographic, near plane is m3 + m2
        frustum_planes[4] = Vector4{
            g_render.cameras.c3d.mat_view_proj.m3 + g_render.cameras.c3d.mat_view_proj.m2,
            g_render.cameras.c3d.mat_view_proj.m7 + g_render.cameras.c3d.mat_view_proj.m6,
            g_render.cameras.c3d.mat_view_proj.m11 + g_render.cameras.c3d.mat_view_proj.m10,
            g_render.cameras.c3d.mat_view_proj.m15 + g_render.cameras.c3d.mat_view_proj.m14,
        };
    } else {
        // For perspective, near plane is just m2
        frustum_planes[4] = Vector4{
            g_render.cameras.c3d.mat_view_proj.m2,
            g_render.cameras.c3d.mat_view_proj.m6,
            g_render.cameras.c3d.mat_view_proj.m10,
            g_render.cameras.c3d.mat_view_proj.m14,
        };
    }

    // Far plane
    frustum_planes[5] = Vector4{
        g_render.cameras.c3d.mat_view_proj.m3 - g_render.cameras.c3d.mat_view_proj.m2,
        g_render.cameras.c3d.mat_view_proj.m7 - g_render.cameras.c3d.mat_view_proj.m6,
        g_render.cameras.c3d.mat_view_proj.m11 - g_render.cameras.c3d.mat_view_proj.m10,
        g_render.cameras.c3d.mat_view_proj.m15 - g_render.cameras.c3d.mat_view_proj.m14,
    };

    // Normalize the planes
    for (SZ i = 0; i < RENDER_FRUSTUM_PLANE_COUNT; ++i) {
        F32 const length = math_sqrt_f32(
            (frustum_planes[i].x * frustum_planes[i].x) +
            (frustum_planes[i].y * frustum_planes[i].y) +
            (frustum_planes[i].z * frustum_planes[i].z));
        frustum_planes[i] = Vector4{frustum_planes[i].x / length, frustum_planes[i].y / length, frustum_planes[i].z / length, frustum_planes[i].w / length};
    }
}

void c3d_update_frustum() {
    Camera3D *cam                      = g_render.cameras.c3d.active_cam;
    g_render.cameras.c3d.fov_radians   = cam->fovy * DEG2RAD;
    g_render.cameras.c3d.mat_view      = MatrixLookAt(cam->position, cam->target, cam->up);

    // Use appropriate projection matrix based on camera projection type
    if (cam->projection == CAMERA_ORTHOGRAPHIC) {
        // For orthographic projection, fovy represents the total visible height
        // We need to calculate the view width based on aspect ratio
        F32 const top    = cam->fovy / 2.0F;
        F32 const bottom = -top;
        F32 const right  = top * g_render.aspect_ratio;
        F32 const left   = -right;
        g_render.cameras.c3d.mat_proj = MatrixOrtho(left, right, bottom, top, RENDER_NEAR_PLANE, RENDER_FAR_PLANE);
    } else {
        g_render.cameras.c3d.mat_proj = MatrixPerspective(g_render.cameras.c3d.fov_radians, g_render.aspect_ratio, RENDER_NEAR_PLANE, RENDER_FAR_PLANE);
    }

    g_render.cameras.c3d.mat_view_proj = MatrixMultiply(g_render.cameras.c3d.mat_view, g_render.cameras.c3d.mat_proj);
    i_calculate_frustum_planes(g_render.cameras.c3d.frustum_planes, cam->projection == CAMERA_ORTHOGRAPHIC);

    for (SZ i = 0; i < RENDER_FRUSTUM_PLANE_COUNT; ++i) {
        g_render.cameras.c3d.normals[i] = {
            g_render.cameras.c3d.frustum_planes[i].x,
            g_render.cameras.c3d.frustum_planes[i].y,
            g_render.cameras.c3d.frustum_planes[i].z,
        };
    }
}

BOOL c3d_is_point_in_frustum(Vector3 point) {
    // Check if point is inside all planes
    for (auto plane : g_render.cameras.c3d.frustum_planes) {
        if (((plane.x * point.x) + (plane.y * point.y) + (plane.z * point.z) + plane.w) < 0.0F) { return false; }
    }

    return true;
}

BOOL c3d_is_obb_in_frustum(OrientedBoundingBox bbox) {
    Camera3D *cam = g_render.cameras.c3d.active_cam;

    // Perspective frustum culling using separating axis theorem
    for (SZ i = 0; i < RENDER_FRUSTUM_PLANE_COUNT; ++i) {
        Vector3 const normal = g_render.cameras.c3d.normals[i];

        // Project OBB radius onto plane normal
        F32 const r = (bbox.extents.x * math_abs_f32(Vector3DotProduct(bbox.axes[0], normal))) +
            (bbox.extents.y * math_abs_f32(Vector3DotProduct(bbox.axes[1], normal))) +
            (bbox.extents.z * math_abs_f32(Vector3DotProduct(bbox.axes[2], normal)));

        // Distance from center to plane
        F32 const d = Vector3DotProduct(normal, bbox.center) + g_render.cameras.c3d.frustum_planes[i].w;

        if (d < -r) { return false; }
    }

    return true;
}

Vector2 c3d_world_to_screen(Vector3 position) {
    return GetWorldToScreen(position, *g_render.cameras.c3d.active_cam);
}
