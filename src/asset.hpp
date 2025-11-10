#pragma once

#include "common.hpp"
#include "array.hpp"
#include "string.hpp"

#include <raylib.h>
#include <time.h>
#include <tinycthread.h>

#ifdef call_once
#undef call_once
#endif

fwd_decl_ns(FMOD, Sound);

#define A_PER_TYPE_MAX 512
#define A_RELOAD_THREAD_SLEEP_DURATION_MS 64
#define A_TERRAIN_DEFAULT_SCALE 1.0F
#define A_TERRAIN_DEFAULT_SIZE 1024
#define A_TERRAIN_SAMPLE_RATE 1024
#define A_MODEL_ICON_SIZE 256
#define A_MODEL_SHADER_NAME "model"
#define A_MODEL_INSTANCED_SHADER_NAME "model_instanced"
#define A_SKYBOX_SHADER_NAME "skybox"
#define A_CUBEMAP_SHADER_NAME "cubemap"
#define A_TERRAIN_INFO_SHADER_NAME "terrain_info"
#define A_PATH_MAX_LENGTH 128
#define A_NAME_MAX_LENGTH 32
#define A_ASSETS_PATH "assets"
#define A_MODELS_PATH A_ASSETS_PATH "/models"
#define A_TEXTURES_PATH A_ASSETS_PATH "/textures"
#define A_SKYBOXES_PATH A_ASSETS_PATH "/skyboxes"
#define A_SOUNDS_PATH A_ASSETS_PATH "/sounds"
#define A_SHADERS_PATH A_ASSETS_PATH "/shaders"
#define A_COMPUTE_SHADERS_PATH A_ASSETS_PATH "/shaders"
#define A_FONTS_PATH A_ASSETS_PATH "/fonts"
#define A_TERRAINS_PATH A_ASSETS_PATH "/terrains"

#define A_DEFAULT_FONT "GoMono"
#define A_DEFAULT_FONT_SIZE 14

#define A_BLOB_VERSION 1
#define A_BLOB_FILE_PATH "blob.bin"

#if A_TERRAIN_SAMPLE_RATE > A_TERRAIN_DEFAULT_SIZE
#error "Sample rate must be less than default size"
#endif

enum AType : U8 {
    A_TYPE_MODEL,
    A_TYPE_TEXTURE,
    A_TYPE_SOUND,
    A_TYPE_SHADER,
    A_TYPE_COMPUTE_SHADER,
    A_TYPE_FONT,
    A_TYPE_SKYBOX,
    A_TYPE_TERRAIN,
    A_TYPE_COUNT,
};

struct AHeader {
    BOOL loaded;
    AType type;
    C8 path[A_PATH_MAX_LENGTH];
    C8 name[A_NAME_MAX_LENGTH];
    U32 name_hash;         // Cached hash of name (for fast lookups)
    time_t last_access;
    time_t last_modified;
    BOOL want_reload;
    SZ file_size;
};

struct CollisionMesh {
    F32 *vertices;           // Simplified collision vertices (x,y,z packed)
    U32 *indices;            // Triangle indices
    U32 vertex_count;
    U32 triangle_count;
    BOOL generated;          // Whether collision mesh was generated
};

struct AModel {
    AHeader header;
    Model base;
    ModelAnimation *animations;
    S32 animation_count;
    BOOL has_animations;      // True if model has animations and bones (computed once on load)
    BoundingBox bb;
    SZ vertex_count;
    CollisionMesh collision;  // Simplified collision mesh for physics
    Texture2D icon;
};

struct ATexture {
    AHeader header;
    Texture2D base;
    Image image;
};

struct ASound {
    AHeader header;
    FMOD::Sound *base;
    U32 length;
};

struct AShader {
    AHeader header;
    Shader base;
};

struct AComputeShader {
    AHeader header;
    Shader base;
};

struct AFont {
    AHeader header;
    S32 font_size;
    Font base;
};

struct ASkybox {
    AHeader header;
    Mesh mesh;
    Model model;
    ATexture *texture;
    Color main_texture_color;
    AShader *skybox_shader;
    AShader *cubemap_shader;
};

struct ATerrain {
    AHeader header;
    Vector3 dimensions;
    Mesh mesh;
    Model model;
    ATexture *height_texture;
    ATexture *diffuse_texture;
    Color main_diffuse_color;
    Matrix transform;
    F32 *height_field;        // Raw height data
    U32 height_field_width;   // Width of height field
    U32 height_field_height;  // Height of height field

    // Terrain info data
    Material info_material;
    Mesh info_mesh;
    MatrixArray info_transforms;
    Vector3Array info_normals;
    Vector3Array info_positions;
};

struct ABlobHeader {
    U32 version;
    U64 created;
};

struct ABlob {
    U8 *data;
    SZ size;
    C8 filepath[A_PATH_MAX_LENGTH];
};

ARRAY_DECLARE(ABlobArray, ABlob);
ARRAY_DECLARE(ATextureArray, ATexture*);

struct Assets {
    BOOL initialized;
    BOOL run_update_thread;
    thrd_t update_thread;
    mtx_t update_thread_mutex;
    BOOL stop_update_thread;
    BOOL fonts_prepared;

    AModel models                  [A_PER_TYPE_MAX]; SZ model_count;
    ATexture textures              [A_PER_TYPE_MAX]; SZ texture_count;
    ASound sounds                  [A_PER_TYPE_MAX]; SZ sound_count;
    AShader shaders                [A_PER_TYPE_MAX]; SZ shader_count;
    AComputeShader compute_shaders [A_PER_TYPE_MAX]; SZ compute_shader_count;
    AFont fonts                    [A_PER_TYPE_MAX]; SZ font_count;
    ASkybox skyboxes               [A_PER_TYPE_MAX]; SZ skybox_count;
    ATerrain terrains              [A_PER_TYPE_MAX]; SZ terrain_count;

    ABlobArray blobs;
};

Assets extern g_assets;

void asset_init();
void asset_start_reload_thread();
void asset_quit();
void asset_update();
AModel *asset_get_model(C8 const *name);
AModel *asset_get_model_by_hash(U32 name_hash);
ATexture *asset_get_texture(C8 const *name);
ATexture *asset_get_texture_by_hash(U32 name_hash);
ASound *asset_get_sound(C8 const *name);
ASound *asset_get_sound_by_hash(U32 name_hash);
AShader *asset_get_shader(C8 const *name);
AShader *asset_get_shader_by_hash(U32 name_hash);
AComputeShader *asset_get_compute_shader(C8 const *name);
AComputeShader *asset_get_compute_shader_by_hash(U32 name_hash);
AFont *asset_get_font(C8 const *name, S32 font_size);
AFont *asset_get_font_by_hash(U32 name_hash, S32 font_size);
ASkybox *asset_get_skybox(C8 const *name);
ASkybox *asset_get_skybox_by_hash(U32 name_hash);
ATerrain *asset_get_terrain(C8 const *name, Vector3 dimensions);
ATerrain *asset_get_terrain_by_hash(U32 name_hash, Vector3 dimensions);
void asset_set_model_shader(AModel *model, Shader shader);
void asset_set_model_shader_rl(Model *model, Shader shader);
void asset_set_skybox_shader(ASkybox *skybox, Shader shader);
void asset_set_terrain_shader(ATerrain *terrain, Shader shader);
void asset_print_state();
C8 const *asset_type_to_cstr(AType type);

void asset_blob_init();
void asset_blob_write();
void asset_blob_read();
void asset_blob_unload();

F32 asset_get_animation_duration(C8 const *model_name, U32 anim_index, F32 fps, F32 anim_speed);
F32 asset_get_animation_duration_by_hash(U32 model_name_hash, U32 anim_index, F32 fps, F32 anim_speed);

#define DUMMY_TEXTURE asset_get_texture("texel_checker.png")
