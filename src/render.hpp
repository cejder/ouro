#pragma once

#include "common.hpp"
#include "math.hpp"

#define RENDER_DEFAULT_AMBIENT_COLOR BLACK
#define RENDER_NEAR_PLANE 0.1F
#define RENDER_FAR_PLANE 100000.0F
#define RENDER_DEFAULT_FONT_PERC 1.5F
#define RENDER_DEFAULT_MAJOR_COLOR TUSCAN
#define RENDER_DEFAULT_MINOR_COLOR NAYGREEN
#define RENDER_DEFAULT_DARKNESS_COLOR NAYGREEN
#define RENDER_DEFAULT_NORMAL_COLOR GREEN
#define RENDER_DEFAULT_TINT_COLOR WHITE
#define RENDER_FRUSTUM_PLANE_COUNT 6

#define RENDER_DEFAULT_LINE_THICKNESS_PERC 0.075F
#define RENDER_WIREFRAME_LINE_THICKNESS_PERC 0.125F

#define CAMERA2D_DEFAULT_ZOOM 1.0F
#define CAMERA2D_MIN_ZOOM 0.1F
#define CAMERA2D_MAX_ZOOM 10.0F
#define CAMERA3D_DEFAULT_FOV 65.0F

fwd_decl(String);
fwd_decl(Loading);
fwd_decl(AFont);
fwd_decl(AModel);
fwd_decl(ATerrain);
fwd_decl(ASkybox);
fwd_decl(CollisionMesh);

#define RMODE_BEGIN(mode) render_begin_render_mode(mode);
#define RMODE_END render_end_render_mode();

// NOTE: We are transitioning to a render mode system where we have like post fx presets.

enum RenderMode : U8 {
    RMODE_3D,
    RMODE_3D_SKETCH,

    RMODE_2D,
    RMODE_2D_SKETCH,

    RMODE_3D_HUD,
    RMODE_3D_HUD_SKETCH,

    RMODE_2D_HUD,
    RMODE_2D_HUD_SKETCH,

    RMODE_3D_DBG,
    RMODE_2D_DBG,

    RMODE_LAST_LAYER,

    RMODE_COUNT
};

struct RenderModeOrderTarget {
    RenderMode order[RMODE_COUNT];
};

struct RenderModeData {
    SZ generation;
    SZ previous_draw_call_count;
    SZ draw_call_count;
    BOOL begun_but_not_ended;
    Color tint_color;
    RenderTexture target;
};

struct RenderCamera3D {
    Camera3D default_cam;
    Camera3D *active_cam;
    F32 fov_radians;
    Matrix mat_view;
    Matrix mat_proj;
    Matrix mat_view_proj;
    Vector4 frustum_planes[RENDER_FRUSTUM_PLANE_COUNT];
    Vector3 normals[RENDER_FRUSTUM_PLANE_COUNT];
};

struct RenderCamera2D {
    Camera2D default_cam;
    Camera2D *active_cam;
};

struct RenderCameras {
    RenderCamera2D c2d;
    RenderCamera3D c3d;
};

struct RenderSketchEffect {
    Color major_color;
    Color minor_color;
    AShader *shader;
    S32 resolution_uniform_location;
    S32 major_color_uniform_location;
    S32 minor_color_uniform_location;
    S32 time_uniform_location;
};

struct Render {
    BOOL initialized;

    SZ request_count;
    SZ request_capacity;

    RenderSketchEffect sketch;

    BOOL wireframe_mode;
    F32 aspect_ratio;

    Color accent_color;

    RenderTexture final_render_target;
    Material default_material;

    RenderCameras cameras;

    RenderMode begun_rmode;
    RenderMode rmode_order[RMODE_COUNT];
    RenderModeData rmode_data[RMODE_COUNT];

    AFont *default_font;
    ATexture *default_crosshair;
    ATexture *speaker_icon_texture;
    ATexture *camera_icon_texture;

    AShader *skybox_shader;
    F32 ambient_color[4];
    S32 skybox_ambient_color_loc;

    AShader *model_shader;
    S32 model_animation_enabled_loc;
};

extern Render g_render;

void render_init();
void render_update(F32 dt);
void render_update_window_resolution(Vector2 new_res);
void render_update_render_resolution(Vector2 new_res);
void render_begin();
void render_begin_render_mode(RenderMode mode);
void render_end_render_mode();
void render_end();
void render_post();
void render_draw_2d_dbg(Camera3D *camera);
void render_draw_3d_dbg(Camera3D *camera);
Vector2 render_get_center_of_render();
Vector2 render_get_center_of_window();
Vector2 render_get_render_resolution();
Vector2 render_get_window_resolution();

// Extensions
BOOL render_load_bindless_texture_extension();
U64 render_get_texture_handle(U32 texture_id);
void render_make_texture_resident(U64 handle);
void render_make_texture_nonresident(U64 handle);

// UI scaling helpers that apply DPI scaling
S32 ui_font_size(F32 percentage);
F32 ui_scale_x(F32 percentage);
F32 ui_scale_y(F32 percentage);
Vector2 ui_shadow_offset(F32 x_perc, F32 y_perc);
void render_set_accent_color(Color color);
void render_set_ambient_color(Color color);
Color render_get_ambient_color();
void render_sketch_set_major_color(Color color);
void render_sketch_set_minor_color(Color color);
void render_vary_sketch_colors(S32 variation);
void render_sketch_swap_colors();
C8 const *render_mode_to_cstr(RenderMode mode);
void render_set_render_mode_order_for_frame(RenderModeOrderTarget target);
RenderModeOrderTarget render_get_default_render_mode_order();

#define INCREMENT_DRAW_CALL g_render.rmode_data[g_render.begun_rmode].draw_call_count++
#define PIXEL_ALIGN_RECT(rec)  \
    Rectangle {                \
        (F32)(S32)(rec).x,     \
        (F32)(S32)(rec).y,     \
        (F32)(S32)(rec).width, \
        (F32)(S32)(rec).height \
    }

void d2d_loading_screen(Loading *loading, C8 const *label);

void d2d_text(AFont *font, C8 const *text, Vector2 position, Color tint);
void d2d_text_shadow(AFont *font, C8 const *text, Vector2 position, Color tint, Color shadow_color, Vector2 shadow_offset);
void d2d_text_ouc(AFont *font, C8 const *text, Vector2 position, Color default_color);
void d2d_text_ouc_shadow(AFont *font, C8 const *text, Vector2 position, Color default_color, Color shadow_color, Vector2 shadow_offset);
void d2d_texture(ATexture *texture, F32 x, F32 y, Color tint);
void d2d_texture_raylib(Texture2D texture, F32 x, F32 y, Color tint);
void d2d_texture_ex(ATexture *texture, Vector2 position, F32 rotation, F32 scale, Color tint);
void d2d_texture_ex_raylib(Texture2D texture, Vector2 position, F32 rotation, F32 scale, Color tint);
void d2d_texture_pro(ATexture *texture, Rectangle src, Rectangle dst, Vector2 origin, F32 rotation, Color tint);
void d2d_texture_pro_raylib(Texture2D texture, Rectangle src, Rectangle dst, Vector2 origin, F32 rotation, Color tint);
void d2d_texture_v(ATexture *texture, Vector2 position, Color tint);
void d2d_texture_rec(ATexture *texture, Rectangle src, Vector2 position, Color tint);
void d2d_texture_rec_raylib(Texture2D texture, Rectangle src, Vector2 position, Color tint);
void d2d_texture_gl(U32 id, Vector2 position, Vector2 size, Color tint);
void d2d_triangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color);
void d2d_rectangle(S32 x, S32 y, S32 width, S32 height, Color color);
void d2d_rectangle_lines(S32 x, S32 y, S32 width, S32 height, Color color);
void d2d_rectangle_lines_ex(Rectangle rec, F32 line_thickness, Color color);
void d2d_rectangle_v(Vector2 position, Vector2 size, Color color);
void d2d_rectangle_rec(Rectangle rec, Color color);
void d2d_rectangle_rec_lines(Rectangle rec, Color color);
void d2d_rectangle_rounded(S32 x, S32 y, S32 width, S32 height, F32 roundness, S32 segments, Color color);
void d2d_rectangle_rounded_lines(S32 x, S32 y, S32 width, S32 height, F32 roundness, S32 segments, Color color);
void d2d_rectangle_rounded_lines_ex(Rectangle rec, F32 roundness, S32 segments, F32 line_thickness, Color color);
void d2d_rectangle_rounded_v(Vector2 position, Vector2 size, F32 roundness, S32 segments, Color color);
void d2d_rectangle_rounded_rec(Rectangle rec, F32 roundness, S32 segments, Color color);
void d2d_rectangle_rounded_rec_lines(Rectangle rec, F32 roundness, S32 segments, Color color);
void d2d_line_strip(Vector2 *points, SZ point_count, Color color);
void d2d_line(Vector2 start, Vector2 end, Color color);
void d2d_line_ex(Vector2 start, Vector2 end, F32 thick, Color color);
void d2d_circle(Vector2 center, F32 radius, Color color);
void d2d_circle_lines(Vector2 center, F32 radius, Color color);
void d2d_circle_sector(Vector2 center, F32 radius, F32 start_angle, F32 end_angle, S32 segments, Color color);
void d2d_circle_sector_lines(Vector2 center, F32 radius, F32 start_angle, F32 end_angle, S32 segments, Color color);
void d2d_whole_screen(ATexture *texture);
void d2d_gizmo(EID eid);
void d2d_bone_gizmo(EID eid);
void d2d_healthbar(EID eid);

void d3d_cube(Vector3 position, F32 width, F32 height, F32 length, Color color);
void d3d_cube_v(Vector3 position, Vector3 size, Color color);
void d3d_cube_wires(Vector3 position, F32 width, F32 height, F32 length, Color color);
void d3d_sphere(Vector3 center, F32 radius, Color color);
void d3d_sphere_ex(Vector3 center, F32 radius, S32 rings, S32 slices, Color color);
void d3d_sphere_wires(Vector3 center, F32 radius, S32 rings, S32 slices, Color color);
void d3d_line(Vector3 start, Vector3 end, Color color);
void d3d_model_rl(Model *model, Vector3 position, F32 scale, Color tint);
void d3d_model_ex(AModel *model, Vector3 position, F32 rotation, Vector3 scale, Color tint);
void d3d_model_transform_rl(Model *model, Matrix *transform, Color tint);
void d3d_model(C8 const *model_name, Vector3 position, F32 rotation, Vector3 scale, Color tint);
void d3d_model_animated(C8 const *model_name, Vector3 position, F32 rotation, Vector3 scale, Color tint, Matrix *bone_matrices, S32 bone_count);
void d3d_mesh_rl(Mesh *mesh, Material *material, Matrix *transform);
void d3d_mesh_rl_instanced(Mesh *mesh, Material *material, Matrix *transforms, SZ instances);
void d3d_bounding_box(BoundingBox bb, Color color);
void d3d_oriented_bounding_box(OrientedBoundingBox obb, Color color);
void d3d_terrain(ATerrain *terrain, F32 scale, Color tint);
void d3d_terrain_ex(ATerrain *terrain, Vector3 rotation, Vector3 scale, Color tint);
void d3d_plane(Vector3 position, Vector2 size, Color color);
void d3d_skybox(ASkybox *skybox);
void d3d_camera(Camera3D *camera, Color color);
void d3d_billboard(ATexture *texture, Vector3 position, F32 scale, Color tint);
void d3d_gizmo(Vector3 position, F32 rotation, OrientedBoundingBox bbox, Color bbox_color, F32 gizmos_alpha, BOOL has_talker, BOOL is_selected);
void d3d_bone_gizmo(EID entity_id);
void d3d_collision_mesh(CollisionMesh *collision, Vector3 position, Vector3 scale, Color color);

void c2d_set(Camera2D *camera);
Camera2D *c2d_get_ptr();
Camera2D c2d_get();
Camera2D *c2d_get_default_ptr();
Camera2D c2d_get_default();
void c2d_set_position(Vector2 position);
void c2d_set_target(Vector2 target);
void c2d_set_offset(Vector2 offset);
void c2d_add_zoom(F32 amount);
void c2d_set_zoom(F32 amount);
F32 c2d_get_zoom();
void c2d_reset();
void c2d_copy(Camera2D *dst, Camera2D *src);

struct C3DResetData {
    BOOL notify;

    Camera3D *other;
    Vector3 other_positon;
    Vector3 other_target;
    Vector3 other_up;
    F32 other_fovy;
    CameraProjection other_projection;

    Vector3 default_positon;
    Vector3 default_target;
    Vector3 default_up;
    F32 default_fovy;
    CameraProjection default_projection;
};

void c3d_set(Camera3D *camera);
Camera3D *c3d_get_ptr();
Camera3D c3d_get();
Camera3D *c3d_get_default_ptr();
Camera3D c3d_get_default();
void c3d_set_position(Vector3 position);
void c3d_set_target(Vector3 target);
void c3d_set_up(Vector3 up);
F32 c3d_get_fov();
void c3d_reset();
void c3d_cb_reset(void *data);
void c3d_cb_toggle(void *data);
void c3d_copy_from_other(Camera3D *dst, Camera3D *src);
void c3d_pull_default_to_other(Camera3D *src);
BOOL c3d_is_point_in_frustum(Vector3 point);
BOOL c3d_is_obb_in_frustum(OrientedBoundingBox bbox);
Vector2 c3d_world_to_screen(Vector3 position);
