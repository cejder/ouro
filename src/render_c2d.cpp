#include "log.hpp"
#include "render.hpp"

#include <raylib.h>

void c2d_set(Camera2D *camera) {
    g_render.cameras.c2d.active_cam = camera;
}

Camera2D *c2d_get_ptr() {
    return g_render.cameras.c2d.active_cam;
}

Camera2D c2d_get() {
    return *g_render.cameras.c2d.active_cam;
}

Camera2D *c2d_get_default_ptr() {
    return &g_render.cameras.c2d.default_cam;
}

Camera2D c2d_get_default() {
    return g_render.cameras.c2d.default_cam;
}

void c2d_set_position(Vector2 position) {
    g_render.cameras.c2d.active_cam->offset = position;
}

void c2d_set_target(Vector2 target) {
    g_render.cameras.c2d.active_cam->target = target;
}

void c2d_set_offset(Vector2 offset) {
    g_render.cameras.c2d.active_cam->offset = offset;
}

void c2d_add_zoom(F32 amount) {
    if (g_render.cameras.c2d.active_cam->zoom + amount < CAMERA2D_MIN_ZOOM) {
        llw("Zooming out too far, setting to minimum zoom of %f", CAMERA2D_MIN_ZOOM);
        g_render.cameras.c2d.active_cam->zoom = CAMERA2D_MIN_ZOOM;
    } else if (g_render.cameras.c2d.active_cam->zoom + amount > CAMERA2D_MAX_ZOOM) {
        llw("Zooming in too far, setting to maximum zoom of %f", CAMERA2D_MAX_ZOOM);
        g_render.cameras.c2d.active_cam->zoom = CAMERA2D_MAX_ZOOM;
    } else {
        g_render.cameras.c2d.active_cam->zoom += amount;
    }
}

void c2d_set_zoom(F32 amount) {
    if (amount < CAMERA2D_MIN_ZOOM) {
        llw("Zooming out too far, setting to minimum zoom of %f", CAMERA2D_MIN_ZOOM);
        g_render.cameras.c2d.active_cam->zoom = CAMERA2D_MIN_ZOOM;
    } else if (amount > CAMERA2D_MAX_ZOOM) {
        llw("Zooming in too far, setting to maximum zoom of %f", CAMERA2D_MAX_ZOOM);
        g_render.cameras.c2d.active_cam->zoom = CAMERA2D_MAX_ZOOM;
    } else {
        g_render.cameras.c2d.active_cam->zoom = amount;
    }
}

F32 c2d_get_zoom() {
    return g_render.cameras.c2d.active_cam->zoom;
}

void c2d_reset() {
    g_render.cameras.c2d.active_cam           = &g_render.cameras.c2d.default_cam;
    g_render.cameras.c2d.active_cam->target   = {};
    g_render.cameras.c2d.active_cam->offset   = {};
    g_render.cameras.c2d.active_cam->rotation = 0.0F;
    g_render.cameras.c2d.active_cam->zoom     = CAMERA2D_DEFAULT_ZOOM;
}

void c2d_copy(Camera2D *dst, Camera2D *src) {
    dst->offset   = src->offset;
    dst->target   = src->target;
    dst->rotation = src->rotation;
    dst->zoom     = src->zoom;
}
