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
        g_world->player.non_player_camera = true;
    } else {
        c3d_set(cam);
        mio("Switching to \\ouc{#00ff00ff}player camera", WHITE);
        g_world->player.non_player_camera = false;
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

BOOL c3d_is_point_in_frustum(Vector3 point) {
    // Check if point is inside all planes
    for (auto plane : g_render.cameras.c3d.frustum_planes) {
        if (((plane.x * point.x) + (plane.y * point.y) + (plane.z * point.z) + plane.w) < 0.0F) { return false; }
    }

    return true;
}

BOOL c3d_is_obb_in_frustum(OrientedBoundingBox bbox) {
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
