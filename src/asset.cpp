#include "asset.hpp"
#include "arg.hpp"
#include "assert.hpp"
#include "audio.hpp"
#include "color.hpp"
#include "console.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "info.hpp"
#include "log.hpp"
#include "memory.hpp"
#include "message.hpp"
#include "profiler.hpp"
#include "raylib.h"
#include "render.hpp"
#include "scene.hpp"
#include "std.hpp"
#include "string.hpp"
#include "time.hpp"
#include "unit.hpp"

#include <config.h>
#include <raymath.h>
#include <rlgl.h>
#include <external/glad.h>

#define A_MAX_LOAD_TIME_BEFORE_WARNING 0.25F
#define A_MAX_VERTICES_BEFORE_WARNING 10000
#define A_FONT_TTF_DEFAULT_NUM_CHARS 95

Assets g_assets = {};

C8 static const *i_assets_type_to_cstr[A_TYPE_COUNT] = {
    "Model",
    "Texture",
    "Sound",
    "Shader",
    "Font",
    "Skybox",
    "Terrain"
};

void static i_fill_header(AHeader *header, C8 const *path, AType type) {
    if (ou_strlen(path) > A_PATH_MAX_LENGTH) {
        lle("Asset path %s (%zu) is longer than %d characters which is the maximum allowed", path, ou_strlen(path), A_PATH_MAX_LENGTH);
        return;
    }

    C8 const *file_name = nullptr;
    SZ file_size = 0;

    switch (type) {
        case A_TYPE_MODEL:
        case A_TYPE_TEXTURE:
        case A_TYPE_SOUND:
        case A_TYPE_SKYBOX: {
            file_name = GetFileName(path);
            file_size = (SZ)GetFileLength(path);
        } break;

        case A_TYPE_FONT: {
            file_name = GetFileNameWithoutExt(path);
            file_size = (SZ)GetFileLength(path);
        } break;

        case A_TYPE_TERRAIN: {
            file_name              = GetFileName(path);
            C8 const *diffuse_path = TS("%s/diffuse.png", path)->c;
            C8 const *height_path  = TS("%s/height.png", path)->c;
            file_size             += (SZ)GetFileLength(diffuse_path);
            file_size             += (SZ)GetFileLength(height_path);
        } break;

        case A_TYPE_SHADER: {
            file_name       = GetFileNameWithoutExt(path);
            C8 const *vpath = TS("%s/vert.glsl", path)->c;
            C8 const *fpath = TS("%s/frag.glsl", path)->c;
            file_size      += (SZ)GetFileLength(vpath);
            file_size      += (SZ)GetFileLength(fpath);
        } break;

        case A_TYPE_COMPUTE_SHADER: {
            file_name       = GetFileNameWithoutExt(path);
            C8 const *cpath = TS("%s/compute.glsl", path)->c;
            file_size      += (SZ)GetFileLength(cpath);
        } break;

        default: {
            _unreachable_();
        } break;
    }

    ou_strncpy((C8 *)header->path, path, A_PATH_MAX_LENGTH);
    ou_strncpy((C8 *)header->name, file_name, A_NAME_MAX_LENGTH);
    header->file_size     = file_size;
    header->last_modified = GetFileModTime(header->path);
    header->type          = type;
}

// Generate simplified collision mesh from visual mesh
// Strategy: Keep significant geometry (floors/walls), remove visual details
void static i_generate_collision_mesh(AModel *model) {
    model->collision.vertices = nullptr;
    model->collision.indices = nullptr;
    model->collision.vertex_count = 0;
    model->collision.triangle_count = 0;
    model->collision.generated = false;

    U32 total_triangles = 0;
    for (S32 mesh_idx = 0; mesh_idx < model->base.meshCount; ++mesh_idx) {
        Mesh *mesh = &model->base.meshes[mesh_idx];
        if (mesh->triangleCount > 0) {
            total_triangles += (U32)mesh->triangleCount;
        }
    }

    if (total_triangles == 0) { return; }

    U32 constexpr max_collision_triangles = 10000;
    U32 const collision_tri_count = (total_triangles < max_collision_triangles) ? total_triangles : max_collision_triangles;

    // Use transient arena for temp buffer during generation
    auto *temp_verts = mmta(Vector3*, (SZ)(collision_tri_count) * 3 * sizeof(Vector3));
    U32 vert_count = 0;

    for (S32 mesh_idx = 0; mesh_idx < model->base.meshCount; ++mesh_idx) {
        Mesh *mesh = &model->base.meshes[mesh_idx];
        if (!mesh->vertices || mesh->triangleCount == 0) { continue; }

        BOOL const is_indexed = (mesh->indices != nullptr);
        U32 const tri_count = (U32)mesh->triangleCount;

        for (U32 tri_idx = 0; tri_idx < tri_count && vert_count < collision_tri_count * 3; ++tri_idx) {
            U32 idx0 = 0;
            U32 idx1 = 0;
            U32 idx2 = 0;
            if (is_indexed) {
                idx0 = mesh->indices[(tri_idx * 3) + 0];
                idx1 = mesh->indices[(tri_idx * 3) + 1];
                idx2 = mesh->indices[(tri_idx * 3) + 2];
            } else {
                idx0 = (tri_idx * 3) + 0;
                idx1 = (tri_idx * 3) + 1;
                idx2 = (tri_idx * 3) + 2;
            }

            Vector3 const v0 = {mesh->vertices[(idx0 * 3) + 0], mesh->vertices[(idx0 * 3) + 1], mesh->vertices[(idx0 * 3) + 2]};
            Vector3 const v1 = {mesh->vertices[(idx1 * 3) + 0], mesh->vertices[(idx1 * 3) + 1], mesh->vertices[(idx1 * 3) + 2]};
            Vector3 const v2 = {mesh->vertices[(idx2 * 3) + 0], mesh->vertices[(idx2 * 3) + 1], mesh->vertices[(idx2 * 3) + 2]};

            Vector3 const edge1 = Vector3Subtract(v1, v0);
            Vector3 const edge2 = Vector3Subtract(v2, v0);
            Vector3 const cross = Vector3CrossProduct(edge1, edge2);
            F32 const area = Vector3Length(cross) * 0.5F;

            if (area < 0.01F) { continue; }

            Vector3 const normal = Vector3Normalize(cross);
            F32 const normal_y = normal.y;
            BOOL const is_floor_or_ceiling = (normal_y > 0.7F || normal_y < -0.7F);
            BOOL const is_wall = (normal_y > -0.5F && normal_y < 0.5F);

            if (is_floor_or_ceiling || is_wall) {
                temp_verts[vert_count++] = v0;
                temp_verts[vert_count++] = v1;
                temp_verts[vert_count++] = v2;
            }
        }
    }

    if (vert_count == 0) {
        return;  // No free needed - arena allocator!
    }

    U32 const final_tri_count = vert_count / 3;
    model->collision.vertex_count = vert_count;
    model->collision.triangle_count = final_tri_count;

    // Use permanent arena for collision mesh - lives with the asset
    model->collision.vertices = mmpa(F32*, (SZ)(vert_count) * 3 * sizeof(F32));
    for (U32 i = 0; i < vert_count; ++i) {
        model->collision.vertices[(i * 3) + 0] = temp_verts[i].x;
        model->collision.vertices[(i * 3) + 1] = temp_verts[i].y;
        model->collision.vertices[(i * 3) + 2] = temp_verts[i].z;
    }

    model->collision.indices = mmpa(U32*, vert_count * sizeof(U32));
    for (U32 i = 0; i < vert_count; ++i) {
        model->collision.indices[i] = i;
    }

    model->collision.generated = true;
    // No free needed - temp_verts in TARENA gets cleared automatically!

    lli("Collision mesh for %s: %u tris (from %u visual)", model->header.name, final_tri_count, total_triangles);
}

void static i_load_model(C8 const *path) {
    AModel *a = &g_assets.models[g_assets.model_count++];
    i_fill_header(&a->header, path, A_TYPE_MODEL);

    a->base = LoadModel(a->header.path);
    if (!IsModelValid(a->base)) {
        lle("Could not load asset %s", a->header.path);
        return;
    }

    a->bb = GetModelBoundingBox(a->base);

    // Filter and shader
    for (S32 i = 0; i < a->base.materialCount; ++i) {
        for (S32 j = 0; j < MAX_MATERIAL_MAPS; ++j) {
            if (a->base.materials[i].maps[j].texture.id > 0) {
                SetTextureFilter(a->base.materials[i].maps[j].texture, TEXTURE_FILTER_ANISOTROPIC_16X);
                GenTextureMipmaps(&a->base.materials[i].maps[j].texture);
            }
        }
    }

    AShader *model_shader = asset_get_shader(A_MODEL_SHADER_NAME);
    asset_set_model_shader(a, model_shader->base);

    for (S32 i = 0; i < a->base.meshCount; ++i) { a->vertex_count += (SZ)a->base.meshes[i].vertexCount; }

    if (a->vertex_count > A_MAX_VERTICES_BEFORE_WARNING) { llw("Model %s has more than %d vertices.", a->header.name, A_MAX_VERTICES_BEFORE_WARNING); }

    // Generate simplified collision mesh for physics
    i_generate_collision_mesh(a);

    // Load animation related stuff
    a->animations = LoadModelAnimations(a->header.path, &a->animation_count);

    // Compute has_animations flag once on load
    a->has_animations = (a->animation_count > 0 && a->base.boneCount > 0);

    // Generate icon
    F32 radius     = Vector3Distance(a->bb.min, a->bb.max) * 0.5F; // Bounding sphere radius
    F32 distance   = radius * 1.25F;
    Vector3 center = {
        (a->bb.min.x + a->bb.max.x) * 0.5F,
        (a->bb.min.y + a->bb.max.y) * 0.5F,
        (a->bb.min.z + a->bb.max.z) * 0.5F
    };

    Camera3D cam   = {};
    cam.position   = Vector3Add(center, (Vector3){distance, distance * 0.5F, distance});
    cam.target     = center;
    cam.up         = {0.0F, -1.0F, 0.0F}; // We do -1 here since we would have to otherwise rotate the texture.
    cam.fovy       = 45.0F;
    cam.projection = CAMERA_PERSPECTIVE;

    RenderTexture2D target = LoadRenderTexture(A_MODEL_ICON_SIZE, A_MODEL_ICON_SIZE);
    BeginTextureMode(target);
    ClearBackground(BLANK);
    BeginMode3D(cam);
    DrawModel(a->base, {}, 1.0F, WHITE);
    EndMode3D();
    EndTextureMode();

    a->icon = target.texture;

    a->header.loaded = true;
}

void static i_load_texture(C8 const *path) {
    ATexture *a = &g_assets.textures[g_assets.texture_count++];
    i_fill_header(&a->header, path, A_TYPE_TEXTURE);

    a->base = LoadTexture(a->header.path);
    if (!IsTextureValid(a->base)) {
        lle("Could not load asset %s", a->header.path);
        return;
    }

    a->image = LoadImage(a->header.path);
    if (!IsImageValid(a->image)) {
        lle("Could not load image %s", a->header.path);
        return;
    }

    SetTextureFilter(a->base, TEXTURE_FILTER_ANISOTROPIC_16X);
    GenTextureMipmaps(&a->base);

    a->header.loaded = true;
}

void static i_load_sound(C8 const *path) {
    ASound *a = &g_assets.sounds[g_assets.sound_count++];
    i_fill_header(&a->header, path, A_TYPE_SOUND);

    FMOD::System *s = audio_get_fmod_system();
    FMOD_RESULT r = s->createSound(a->header.path, FMOD_DEFAULT, nullptr, &a->base);
    if (r != FMOD_OK) {
        lle("Could not load asset %s", a->header.path);
        return;
    }

    r = a->base->getLength(&a->length, FMOD_TIMEUNIT_MS);
    if (r != FMOD_OK) {
        lle("Could not get length for sound %s", a->header.path);
        return;
    }

    a->header.loaded = true;
}

void static i_load_shader_part2(AShader *a) {
    C8 const *vpath = TS("%s/vert.glsl", a->header.path)->c;
    C8 const *fpath = TS("%s/frag.glsl", a->header.path)->c;

    if (!FileExists(vpath) && !FileExists(fpath)) {
        lle("Vertex and fragment shader not found for %s", a->header.path);
        return;
    }

    // We get the mod time for shaders later since the path is not relevant modtime wise.
    // We grab the newest mod time of the two files.
    a->header.last_modified = glm::max(GetFileModTime(vpath), GetFileModTime(fpath));

    if (!FileExists(vpath)) {
        a->base = LoadShader(nullptr, fpath);
    } else if (!FileExists(fpath)) {
        a->base = LoadShader(vpath, nullptr);
    } else {
        a->base = LoadShader(vpath, fpath);
    }

    if (!IsShaderValid(a->base)) {
        lle("Could not load asset %s", a->header.path);
        return;
    }

    a->header.loaded = true;
}

void static i_load_shader_part1(C8 const *path) {
    AShader *a = &g_assets.shaders[g_assets.shader_count++];
    i_fill_header(&a->header, path, A_TYPE_SHADER);
    i_load_shader_part2(a);
}

void static i_load_compute_shader(C8 const *path) {
    AComputeShader *a = &g_assets.compute_shaders[g_assets.compute_shader_count++];
    i_fill_header(&a->header, path, A_TYPE_COMPUTE_SHADER);

    C8 const *cpath = TS("%s/compute.glsl", a->header.path)->c;

    if (!FileExists(cpath)) {
        lle("Compute shader file not found: %s", cpath);
        return;
    }

    C8* compute_source = LoadFileText(cpath);
    if (!compute_source) {
        lle("Failed to load compute shader file: %s", cpath);
        return;
    }

    // Create compute shader manually with OpenGL
    U32 const shader_id = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader_id, 1, &compute_source, nullptr);
    glCompileShader(shader_id);

    // Check compilation status
    S32 success = S32_MAX;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
    if (!success) {
        C8 info_log[512];
        glGetShaderInfoLog(shader_id, 512, nullptr, info_log);
        lle("Compute shader compilation failed: %s", info_log);
        glDeleteShader(shader_id);
        UnloadFileText(compute_source);
        return;
    }

    // Create program and link
    U32 const program_id = glCreateProgram();
    glAttachShader(program_id, shader_id);
    glLinkProgram(program_id);

    // Check linking status
    glGetProgramiv(program_id, GL_LINK_STATUS, &success);
    if (!success) {
        C8 info_log[512];
        glGetProgramInfoLog(program_id, 512, nullptr, info_log);
        lle("Compute shader program linking failed: %s", info_log);
        glDeleteProgram(program_id);
        glDeleteShader(shader_id);
        UnloadFileText(compute_source);
        return;
    }

    // Clean up
    glDeleteShader(shader_id);
    UnloadFileText(compute_source);

    a->base.id = program_id;

    a->header.loaded = true;
}

// Fonts have a helper static function to load them since we might need to load them on demand.
AFont static *i_load_font(C8 const *path, S32 font_size) {
    AFont *a = &g_assets.fonts[g_assets.font_count++];
    S32 file_size = 0;
    U8 *font_data = LoadFileData(path, &file_size);
    if (font_data == nullptr) {
        lle("Could not load font %s", path);
        return nullptr;
    }

    Font *f       = &a->base;
    f->baseSize   = font_size;
    f->glyphCount = A_FONT_TTF_DEFAULT_NUM_CHARS;
    f->glyphs     = LoadFontData(font_data, file_size, f->baseSize, nullptr, A_FONT_TTF_DEFAULT_NUM_CHARS, FONT_DEFAULT, &f->glyphCount);
    if (f->glyphs == nullptr) {
        lle("Could not load font data for %s", path);
        return nullptr;
    }

    Image const atlas = GenImageFontAtlas(f->glyphs, &f->recs, f->glyphCount, f->baseSize, 4, 0);
    f->texture        = LoadTextureFromImage(atlas);

    UnloadImage(atlas);
    UnloadFileData(font_data);

    // NOTE: We do not check for duplicates here since we load fonts on demand
    // with different sizes and use name and size as a key.

    i_fill_header(&a->header, path, A_TYPE_FONT);
    a->font_size     = font_size;
    a->header.loaded = true;

    return a;
}

void static i_prepare_font(C8 const *path) {
    AFont *a = &g_assets.fonts[g_assets.font_count++];
    i_fill_header(&a->header, path, A_TYPE_FONT);
}

void static i_prepare_fonts() {
    lli("Preparing fonts from %s", A_FONTS_PATH);

    FilePathList const list = LoadDirectoryFiles(A_FONTS_PATH);
    for (SZ i = 0; i < list.count; ++i) { i_prepare_font(list.paths[i]); }

    UnloadDirectoryFiles(list);
}

TextureCubemap static i_skybox_gen_cubemap(ASkybox *a) {
    TextureCubemap cubemap   = {};
    S32 const size           = 1024;
    PixelFormat const format = PIXELFORMAT_UNCOMPRESSED_R32G32B32;
    S32 const mipmaps        = 1;

    rlDisableBackfaceCulling();  // Disable backface culling to render inside the cube.

    // Step 1: Setup framebuffer.
    U32 const rbo = rlLoadTextureDepth(size, size, true);
    cubemap.id    = rlLoadTextureCubemap(nullptr, size, format, mipmaps);

    U32 const fbo = rlLoadFramebuffer();
    rlFramebufferAttach(fbo, rbo, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    rlFramebufferAttach(fbo, cubemap.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_CUBEMAP_POSITIVE_X, 0);

    // Check if framebuffer is complete with attachments (valid).
    if (!rlFramebufferComplete(fbo)) {
        lle("Failed to create framebuffer for cubemap generation.");
        return cubemap;
    }

    // Step 2: Draw to framebuffer.
    // NOTE: Shader is used to convert HDR equirectangular environment map to cubemap equivalent.
    rlEnableShader(a->cubemap_shader->base.id);

    // Define projection matrix and send it to shader.
    Matrix const mat_projection = MatrixPerspective(90.0 * (F64)DEG2RAD, 1.0, rlGetCullDistanceNear(), rlGetCullDistanceFar());
    rlSetUniformMatrix(a->cubemap_shader->base.locs[SHADER_LOC_MATRIX_PROJECTION], mat_projection);

    // Define view matrix and send it to shader.
    Matrix const fbo_views[6] = {
        MatrixLookAt({}, {1.0F, 0.0F, 0.0F}, {0.0F, -1.0F, 0.0F}), MatrixLookAt({}, {-1.0F, 0.0F, 0.0F}, {0.0F, -1.0F, 0.0F}),
        MatrixLookAt({}, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}),  MatrixLookAt({}, {0.0F, -1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}),
        MatrixLookAt({}, {0.0F, 0.0F, 1.0F}, {0.0F, -1.0F, 0.0F}), MatrixLookAt({}, {0.0F, 0.0F, -1.0F}, {0.0F, -1.0F, 0.0F}),
    };

    // Set viewport to current FBO dimensions.
    rlViewport(0, 0, size, size);

    // Activate and enable texture for drawing to cubemap faces.
    rlActiveTextureSlot(0);
    rlEnableTexture(a->texture->base.id);

    for (S32 i = 0; i < 6; ++i) {
        // Set the view matrix for the current face.
        rlSetUniformMatrix(a->cubemap_shader->base.locs[SHADER_LOC_MATRIX_VIEW], fbo_views[i]);

        // Select the current cubemap face attachment for the FBO.
        rlFramebufferAttach(fbo, cubemap.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_CUBEMAP_POSITIVE_X + i, 0);
        rlEnableFramebuffer(fbo);

        // Load and draw a cube, it uses the currently enabled texture.
        rlClearScreenBuffers();
        rlLoadDrawCube();
    }

    // Step 3: Unload framebuffer and reset state.
    rlDisableShader();
    rlDisableTexture();
    rlDisableFramebuffer();
    rlUnloadFramebuffer(fbo);  // Also unloads automatically attached texture and renderbuffer.

    rlViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    rlEnableBackfaceCulling();

    cubemap.width = size;
    cubemap.height = size;
    cubemap.mipmaps = 1;
    cubemap.format = format;

    return cubemap;
}

void static i_load_skybox(C8 const *path) {
    ASkybox *a = &g_assets.skyboxes[g_assets.skybox_count++];
    i_fill_header(&a->header, path, A_TYPE_SKYBOX);

    a->mesh                      = GenMeshCube(1.0F, 1.0F, 1.0F);
    a->model                     = LoadModelFromMesh(a->mesh);
    a->skybox_shader             = asset_get_shader(A_SKYBOX_SHADER_NAME);
    a->cubemap_shader            = asset_get_shader(A_CUBEMAP_SHADER_NAME);
    a->model.materials[0].shader = a->skybox_shader->base;

    SetShaderValue(a->model.materials[0].shader, GetShaderLocation(a->model.materials[0].shader, "environmentMap"), (S32[1]){MATERIAL_MAP_CUBEMAP}, SHADER_UNIFORM_INT);
    SetShaderValue(a->model.materials[0].shader, GetShaderLocation(a->model.materials[0].shader, "doGamma"),        (S32[1]){1},                    SHADER_UNIFORM_INT);
    SetShaderValue(a->model.materials[0].shader, GetShaderLocation(a->model.materials[0].shader, "vflipped"),       (S32[1]){1},                    SHADER_UNIFORM_INT);
    SetShaderValue(a->cubemap_shader->base,      GetShaderLocation(a->cubemap_shader->base, "equirectangularMap"),  (S32[1]){0},                    SHADER_UNIFORM_INT);

    i_load_texture(path);
    a->texture = asset_get_texture(GetFileName(a->header.path));
    SetTextureFilter(a->texture->base, TEXTURE_FILTER_BILINEAR);
    a->model.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = i_skybox_gen_cubemap(a);
    a->main_texture_color = color_from_texture(a->texture);
    a->header.loaded      = true;
}

void static i_delete_outdated_terrain_caches(C8 const *path, S64 heightmap_modtime) {
    // Get all .bin files
    FilePathList const files = LoadDirectoryFilesEx(path, ".bin", false);

    for (U32 i = 0U; i < files.count; i++) {
        C8 const *filename = GetFileName(files.paths[i]);
        // Check if this is one of our cache files
        if (ou_strncmp(filename, "height_cache_", 12) == 0) {
            // Find the modtime in the filename (after the last underscore)
            C8 const *modtime_str = ou_strrchr(filename, '_') + 1;
            // Remove .bin extension before converting
            String const *number_str = TS("%.*s", (S32)(ou_strlen(modtime_str) - 4), modtime_str);
            S64 const cache_modtime = ou_strtol(number_str->c, nullptr, 10);

            if (cache_modtime < heightmap_modtime) {
                remove(files.paths[i]);
                lld("Deleted outdated terrain cache (reason: modtime): %s", files.paths[i]);
            }

            // Check if the dimensions differ
            String *t = TS("%s", filename);
            if (!string_contains(t, TS("%d_%d", A_TERRAIN_DEFAULT_SIZE, A_TERRAIN_SAMPLE_RATE))) {  // HACK: But good enough I guess
                remove(files.paths[i]);
                lld("Deleted outdated terrain cache (reason: size/sample rate): %s", files.paths[i]);
            }
        }
    }

    UnloadDirectoryFiles(files);
}

struct ITerrainThreadData {
    ATerrain *terrain;
    Vector3 dimensions;
    U32 start_z;
    U32 end_z;
};

S32 static i_terrain_thread_func(void *arg) {
    auto *data = (ITerrainThreadData *)arg;
    ATerrain *a = data->terrain;
    Vector3 const dimensions = data->dimensions;

    for (U32 z = data->start_z; z < data->end_z; z++) {
        U32 const z_idx = z * A_TERRAIN_SAMPLE_RATE;
        for (U32 x = 0U; x < A_TERRAIN_SAMPLE_RATE; x++) {
            U32 const idx                = z_idx + x;
            F32 const world_x            = ((F32)x / (F32)(A_TERRAIN_SAMPLE_RATE - 1U)) * dimensions.x;
            F32 const world_z            = ((F32)z / (F32)(A_TERRAIN_SAMPLE_RATE - 1U)) * dimensions.z;
            Vector3 const ray_start      = {world_x, dimensions.y * 2.0F, world_z};
            RayCollision const collision = math_ray_collision_to_terrain(a, ray_start, {0.0F, -1.0F, 0.0F});
            a->height_field[idx] = collision.hit ? collision.point.y : 0.0F;
        }
    }

    return 0;
}

void static i_load_terrain(C8 const *path, Vector3 dimensions) {
    ATerrain *a = &g_assets.terrains[g_assets.terrain_count++];
    i_fill_header(&a->header, path, A_TYPE_TERRAIN);
    a->dimensions = dimensions;

    String *height_path  = TS("%s/%s", path, "height.png");
    String *diffuse_path = TS("%s/%s", path, "diffuse.png");

    // Create unique texture names by prefixing with terrain name
    String *terrain_name        = TS("%s", GetFileName(path));
    String *unique_height_name  = TS("%s_height.png", terrain_name->c);
    String *unique_diffuse_name = TS("%s_diffuse.png", terrain_name->c);

    // Load textures with unique names for each terrain
    ATexture *height_tex = &g_assets.textures[g_assets.texture_count++];
    i_fill_header(&height_tex->header, height_path->c, A_TYPE_TEXTURE);
    ou_strncpy((C8 *)height_tex->header.name, unique_height_name->c, A_NAME_MAX_LENGTH);
    height_tex->base = LoadTexture(height_path->c);
    if (!IsTextureValid(height_tex->base)) {
        lle("Could not load height texture %s", height_path->c);
        return;
    }
    height_tex->image = LoadImage(height_path->c);
    if (!IsImageValid(height_tex->image)) {
        lle("Could not load height image %s", height_path->c);
        return;
    }

    ATexture *diffuse_tex = &g_assets.textures[g_assets.texture_count++];
    i_fill_header(&diffuse_tex->header, diffuse_path->c, A_TYPE_TEXTURE);
    ou_strncpy((C8 *)diffuse_tex->header.name, unique_diffuse_name->c, A_NAME_MAX_LENGTH);
    diffuse_tex->base = LoadTexture(diffuse_path->c);
    if (!IsTextureValid(diffuse_tex->base)) {
        lle("Could not load diffuse texture %s", diffuse_path->c);
        return;
    }
    diffuse_tex->image = LoadImage(diffuse_path->c);
    if (!IsImageValid(diffuse_tex->image)) {
        lle("Could not load diffuse image %s", diffuse_path->c);
        return;
    }

    a->height_texture = height_tex;
    SetTextureFilter(a->height_texture->base, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(a->height_texture->base, TEXTURE_WRAP_CLAMP);

    a->diffuse_texture = diffuse_tex;
    SetTextureFilter(a->diffuse_texture->base, TEXTURE_FILTER_ANISOTROPIC_16X);
    GenTextureMipmaps(&a->diffuse_texture->base);
    SetTextureWrap(a->diffuse_texture->base, TEXTURE_WRAP_REPEAT);

    Image const heightmap_image = LoadImage(height_path->c);
    a->height_field_width  = A_TERRAIN_SAMPLE_RATE;
    a->height_field_height = A_TERRAIN_SAMPLE_RATE;

    // Generate cache filename based on heightmap modtime
    S64 const modtime  = GetFileModTime(height_path->c);
    String *cache_path = TS("%s/height_cache_%u_%u_%" PRId64 ".bin", path, A_TERRAIN_DEFAULT_SIZE, A_TERRAIN_SAMPLE_RATE, modtime);

    // Delete old cache files before checking/loading current one
    i_delete_outdated_terrain_caches(path, modtime);

    S32 const expected_size = (S32)(sizeof(F32) * A_TERRAIN_SAMPLE_RATE * A_TERRAIN_SAMPLE_RATE);
    BOOL need_generate      = true;

    // Check if cache exists and has correct size
    if (FileExists(cache_path->c) && GetFileLength(cache_path->c) == expected_size) {
        S32 data_size = 0;
        U8 *file_data = LoadFileData(cache_path->c, &data_size);

        if (file_data) {
            a->height_field = mmpa(F32 *, sizeof(F32) * A_TERRAIN_SAMPLE_RATE * A_TERRAIN_SAMPLE_RATE);
            ou_memcpy(a->height_field, file_data, (SZ)data_size);
            UnloadFileData(file_data);
            need_generate = false;
            lld("Loaded terrain heights from cache: %s", cache_path->c);
        }
    }

    // Generate the mesh (always needed)
    a->mesh  = GenMeshHeightmap(heightmap_image, dimensions);
    a->model = LoadModelFromMesh(a->mesh);
    a->model.materials[0].shader = asset_get_shader(A_MODEL_SHADER_NAME)->base;
    a->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = a->diffuse_texture->base;
    a->transform = MatrixTranslate(0.0F, 0.0F, 0.0F);

    // In i_load_terrain, replace the height field generation with:
    if (need_generate) {
        SZ const core_count = info_get_cpu_core_count();

        lld("Generating terrain heights with %zu threads for %s", core_count, path);

        U32 const sample_count = A_TERRAIN_SAMPLE_RATE * A_TERRAIN_SAMPLE_RATE;
        a->height_field = mmpa(F32 *, sizeof(F32) * sample_count);

        auto *threads = mmta(thrd_t *, sizeof(thrd_t) * core_count);
        auto *thread_data = mmta(ITerrainThreadData *, sizeof(ITerrainThreadData) * core_count);

        U32 const rows_per_thread = A_TERRAIN_SAMPLE_RATE / core_count;
        U32 const remaining_rows  = A_TERRAIN_SAMPLE_RATE % core_count;
        U32 current_row           = 0;

        for (U32 i = 0; i < core_count; i++) {
            thread_data[i].terrain    = a;
            thread_data[i].dimensions = dimensions;
            thread_data[i].start_z    = current_row;
            thread_data[i].end_z      = current_row + rows_per_thread + (i < remaining_rows ? 1 : 0);
            current_row               = thread_data[i].end_z;

            if (thrd_create(&threads[i], i_terrain_thread_func, &thread_data[i]) != thrd_success) {
                lld("Failed to create thread %u", i);
                return;
            }
        }

        for (U32 i = 0; i < core_count; i++) {
            S32 res = S32_MAX;
            thrd_join(threads[i], &res);
            lld("Thread %u/%zu completed", i + 1, core_count);
        }

        SaveFileData(cache_path->c, a->height_field, (S32)(sizeof(F32) * sample_count));
        lld("Saved terrain heights to cache: %s", cache_path->c);
    }

    a->main_diffuse_color = color_from_texture(a->diffuse_texture);

    UnloadImage(heightmap_image);

    // Generate terrain info data
    SZ const cap = (SZ)A_TERRAIN_DEFAULT_SIZE * (SZ)A_TERRAIN_DEFAULT_SIZE;
    array_init(MEMORY_TYPE_PARENA, &a->info_positions,  cap);
    array_init(MEMORY_TYPE_PARENA, &a->info_normals,    cap);
    array_init(MEMORY_TYPE_PARENA, &a->info_transforms, cap);

    U32 r = A_TERRAIN_DEFAULT_SIZE / A_TERRAIN_SAMPLE_RATE;  // NOLINT
    F32 const normal_length = 2.0F;

    a->info_material = g_render.default_material;
    a->info_mesh = GenMeshSphere(0.25F, 8, 8);
    F32 const min_height = math_find_lowest_point_in_model (&a->model, a->transform).y;
    F32 const max_height = math_find_highest_point_in_model(&a->model, a->transform).y;

    for (U32 z = 0U; z < A_TERRAIN_DEFAULT_SIZE; z += r) {
        for (U32 x = 0U; x < A_TERRAIN_DEFAULT_SIZE; x += r) {
            F32 const height       = math_get_terrain_height(a, (F32)x, (F32)z);
            Vector3 const normal   = math_get_terrain_normal(a, (F32)x, (F32)z);
            Vector3 const position = {(F32)x, height, (F32)z};

            array_push(&a->info_positions, position);
            array_push(&a->info_normals, Vector3Scale(normal, normal_length));
            array_push(&a->info_transforms, MatrixTranslate((F32)x, height, (F32)z));
        }
    }

    AShader *terrain_info_shader = asset_get_shader("terrain_info");
    a->info_material.shader = terrain_info_shader->base;
    terrain_info_shader->base.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(terrain_info_shader->base, "mvp");
    S32 const min_height_loc = GetShaderLocation(terrain_info_shader->base, "minHeight");
    S32 const max_height_loc = GetShaderLocation(terrain_info_shader->base, "maxHeight");
    SetShaderValue(a->info_material.shader, min_height_loc, &min_height, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->info_material.shader, max_height_loc, &max_height, SHADER_UNIFORM_FLOAT);

    a->header.loaded = true;
}

void static i_shader_check_if_reload_needed(AShader *asset) {
    // We only check for reload if the asset has been loaded and is not marked for reload already.
    if (asset->header.last_access == 0 || asset->header.want_reload) { return; }

    C8 vpath[A_PATH_MAX_LENGTH];
    C8 fpath[A_PATH_MAX_LENGTH];
    ou_sprintf(vpath, "%s/vert.glsl", asset->header.path);
    ou_sprintf(fpath, "%s/frag.glsl", asset->header.path);

    time_t v_last_modified = GetFileModTime(vpath);
    time_t f_last_modified = GetFileModTime(fpath);

    // NOTE: One of the shaders might not exist generally e.g. we have only a fragment shader.
    // That means the last modified time will be 0 for that file and we need to check
    // the other file.

    if (v_last_modified == 0) { v_last_modified = f_last_modified; }
    if (f_last_modified == 0) { f_last_modified = v_last_modified; }

    if (v_last_modified > asset->header.last_modified || f_last_modified > asset->header.last_modified) {
        // Check if the file already exists since it might not been written yet and
        // depending on the file system it might not be available immediately.
        // If both files are missing, we skip the reload and try again next frame.
        if (!FileExists(vpath) && !FileExists(fpath)) {
            llw("Vertex and fragment shader not found for %s", asset->header.path);
            return;
        }

        lli("Reloading shader %s due to modification", asset->header.name);

        mtx_lock(&g_assets.update_thread_mutex);
        { asset->header.want_reload = true; }
        mtx_unlock(&g_assets.update_thread_mutex);

        // We will just update the last modified time to the latest of the two files.
        asset->header.last_modified = glm::max(v_last_modified, f_last_modified);

        // We also need to mark all models, terrains and skyboxes that use this shader for reload.
        for (SZ i = 0; i < g_assets.model_count; ++i) {
            AModel *model = &g_assets.models[i];
            if (model->header.loaded && model->base.materials[0].shader.id == asset->base.id) {
                mtx_lock(&g_assets.update_thread_mutex);
                model->header.want_reload = true;
                mtx_unlock(&g_assets.update_thread_mutex);
            }
        }

        for (SZ i = 0; i < g_assets.terrain_count; ++i) {
            ATerrain *terrain = &g_assets.terrains[i];
            if (terrain->header.loaded && terrain->model.materials[0].shader.id == asset->base.id) {
                mtx_lock(&g_assets.update_thread_mutex);
                terrain->header.want_reload = true;
                mtx_unlock(&g_assets.update_thread_mutex);
            }
        }

        for (SZ i = 0; i < g_assets.skybox_count; ++i) {
            ASkybox *skybox = &g_assets.skyboxes[i];
            if (skybox->header.loaded && skybox->model.materials[0].shader.id == asset->base.id) {
                mtx_lock(&g_assets.update_thread_mutex);
                skybox->header.want_reload = true;
                mtx_unlock(&g_assets.update_thread_mutex);
            }
        }
    }
}

void static i_shader_reload(AShader *asset) {
    UnloadShader(asset->base);
    i_load_shader_part2(asset);

    // INFO: We will just force a resolution update for now since it MIGHT be a shader
    // that is used for rendering. This should not cause any major issues or
    // slowdown since it is only done when a shader is reloaded.
    render_update_render_resolution({(F32)c_video__render_resolution_width, (F32)c_video__render_resolution_height});
}

void static i_shaders_apply_reloads_if_needed() {
    for (SZ i = 0; i < g_assets.shader_count; ++i) {
        AShader *asset = &g_assets.shaders[i];
        if (asset->header.want_reload) {
            i_shader_reload(asset);
            mtx_lock(&g_assets.update_thread_mutex);
            { asset->header.want_reload = false; }
            mtx_unlock(&g_assets.update_thread_mutex);

            BOOL warning = false;

            // Now we iterate over all models, terrains and skyboxes to see if they need to be reloaded.
            // We just reassign the shader to the model for now.
            for (SZ j = 0; j < g_assets.model_count; ++j) {
                AModel *model = &g_assets.models[j];
                if (model->header.want_reload) {
                    mtx_lock(&g_assets.update_thread_mutex);
                    {
                        asset_set_model_shader(model, asset->base);
                        model->header.want_reload = false;
                        warning                   = true;
                    }
                    mtx_unlock(&g_assets.update_thread_mutex);
                }
            }

            for (SZ j = 0; j < g_assets.terrain_count; ++j) {
                ATerrain *terrain = &g_assets.terrains[j];
                if (terrain->header.want_reload) {
                    mtx_lock(&g_assets.update_thread_mutex);
                    {
                        asset_set_terrain_shader(terrain, asset->base);
                        terrain->header.want_reload = false;
                        warning                     = true;
                    }
                    mtx_unlock(&g_assets.update_thread_mutex);
                }
            }

            for (SZ j = 0; j < g_assets.skybox_count; ++j) {
                ASkybox *skybox = &g_assets.skyboxes[j];
                if (skybox->header.want_reload) {
                    mtx_lock(&g_assets.update_thread_mutex);
                    {
                        asset_set_skybox_shader(skybox, asset->base);
                        skybox->header.want_reload = false;
                        warning                    = true;
                    }
                    mtx_unlock(&g_assets.update_thread_mutex);
                }
            }

            if (warning) {
                llw("Shader %s was reloaded and we would need to reload models, terrains and skyboxes but this might not work as expected.", asset->header.name);
            }

            mi(TS("Shader %s was reloaded", asset->header.name)->c, GREEN);
        }
    }
}

S32 static i_assets_reload_thread(void *data) {
    unused(data);

    for (;;) {
        for (SZ i = 0; i < g_assets.shader_count; ++i) { i_shader_check_if_reload_needed(&g_assets.shaders[i]); }

        struct timespec const duration = {0, (S64)A_RELOAD_THREAD_SLEEP_DURATION_MS * 1000000};
        struct timespec remaining      = {0, 0};
        thrd_sleep(&duration, &remaining);

        if (g_assets.stop_update_thread) { break; }
    }

    return 0;
}

void asset_init() {
    // asset_blob_init();

    g_assets.run_update_thread = OURO_IS_DEBUG;
    g_assets.initialized       = true;
}

void asset_start_reload_thread() {
    if (g_assets.run_update_thread) {
        mtx_init(&g_assets.update_thread_mutex, mtx_plain);

        if (thrd_create(&g_assets.update_thread, i_assets_reload_thread, nullptr) != thrd_success) {
            lle("Could not create asset reload thread");
            return;
        }
    }
}

void asset_quit() {
    if (OURO_IS_DEBUG) {
        g_assets.stop_update_thread = true;
        thrd_join(g_assets.update_thread, nullptr);
    }
}

void asset_update() {
    if (g_assets.run_update_thread) {
        F32 static last_update = 0.0F;
        F32 const current_time = time_get_glfw();
        if (current_time - last_update < A_RELOAD_THREAD_SLEEP_DURATION_MS / 1000.0F) { return; }
        last_update = current_time;

        i_shaders_apply_reloads_if_needed();
    }
}

AModel *asset_get_model(C8 const *name) {
    for (SZ i = 0; i < g_assets.model_count; ++i) {
        AModel *asset = &g_assets.models[i];
        if (ou_strcmp(asset->header.name, name) == 0) {
            asset->header.last_access = time(nullptr);
            return asset;
        }
    }

    F32 const start_time = time_get_glfw();
    i_load_model(TS("%s/%s", A_MODELS_PATH, name)->c);
    F32 const end_time = time_get_glfw();
    if (end_time - start_time > A_MAX_LOAD_TIME_BEFORE_WARNING) {
        llw("Loading model %s took %.2f seconds and is above the threshold of %.2f seconds.", name, end_time - start_time, A_MAX_LOAD_TIME_BEFORE_WARNING);
    }

    return asset_get_model(name);
}

ATexture *asset_get_texture(C8 const *name) {
    for (SZ i = 0; i < g_assets.texture_count; ++i) {
        ATexture *asset = &g_assets.textures[i];
        if (ou_strcmp(asset->header.name, name) == 0) {
            asset->header.last_access = time(nullptr);
            return asset;
        }
    }

    F32 const start_time = time_get_glfw();
    i_load_texture(TS("%s/%s", A_TEXTURES_PATH, name)->c);
    F32 const end_time = time_get_glfw();
    if (end_time - start_time > A_MAX_LOAD_TIME_BEFORE_WARNING) {
        llw("Loading texture %s took %.2f seconds and is above the threshold of %.2f seconds.", name, end_time - start_time, A_MAX_LOAD_TIME_BEFORE_WARNING);
    }

    return asset_get_texture(name);
}

ASound *asset_get_sound(C8 const *name) {
    for (SZ i = 0; i < g_assets.sound_count; ++i) {
        ASound *asset = &g_assets.sounds[i];
        if (ou_strcmp(asset->header.name, name) == 0) {
            asset->header.last_access = time(nullptr);
            return asset;
        }
    }

    F32 const start_time = time_get_glfw();
    i_load_sound(TS("%s/%s", A_SOUNDS_PATH, name)->c);
    F32 const end_time = time_get_glfw();
    if (end_time - start_time > A_MAX_LOAD_TIME_BEFORE_WARNING) {
        llw("Loading sound %s took %.2f seconds and is above the threshold of %.2f seconds.", name, end_time - start_time, A_MAX_LOAD_TIME_BEFORE_WARNING);
    }

    return asset_get_sound(name);
}

AShader *asset_get_shader(C8 const *name) {
    for (SZ i = 0; i < g_assets.shader_count; ++i) {
        AShader *asset = &g_assets.shaders[i];
        if (ou_strcmp(asset->header.name, name) == 0) {
            asset->header.last_access = time(nullptr);
            return asset;
        }
    }

    F32 const start_time = time_get_glfw();
    i_load_shader_part1(TS("%s/%s", A_SHADERS_PATH, name)->c);
    F32 const end_time = time_get_glfw();
    if (end_time - start_time > A_MAX_LOAD_TIME_BEFORE_WARNING) {
        llw("Loading shader %s took %.2f seconds and is above the threshold of %.2f seconds.", name, end_time - start_time, A_MAX_LOAD_TIME_BEFORE_WARNING);
    }

    return asset_get_shader(name);
}

AComputeShader *asset_get_compute_shader(C8 const *name) {
    for (SZ i = 0; i < g_assets.compute_shader_count; ++i) {
        AComputeShader *asset = &g_assets.compute_shaders[i];
        if (ou_strcmp(asset->header.name, name) == 0) {
            asset->header.last_access = time(nullptr);
            return asset;
        }
    }

    F32 const start_time = time_get_glfw();
    i_load_compute_shader(TS("%s/%s", A_COMPUTE_SHADERS_PATH, name)->c);
    F32 const end_time = time_get_glfw();
    if (end_time - start_time > A_MAX_LOAD_TIME_BEFORE_WARNING) {
        llw("Loading compute shader %s took %.2f seconds and is above the threshold of %.2f seconds.", name, end_time - start_time, A_MAX_LOAD_TIME_BEFORE_WARNING);
    }

    return asset_get_compute_shader(name);
}

AFont *asset_get_font(C8 const *name, S32 font_size) {
    if (font_size < 1) {
        llw("Font size is smaller than 1 (font size: %d), returning default font %s (font size: %d)", font_size, A_DEFAULT_FONT, A_DEFAULT_FONT_SIZE);
        return asset_get_font(A_DEFAULT_FONT, A_DEFAULT_FONT_SIZE);
    }

    if (!g_assets.fonts_prepared) {
        i_prepare_fonts();
        g_assets.fonts_prepared = true;
    }

    AFont *asset = nullptr;

    for (SZ i = 0; i < g_assets.font_count; ++i) {
        if (ou_strcmp(g_assets.fonts[i].header.name, name) == 0) {
            asset = &g_assets.fonts[i];
            if (asset->font_size == font_size) {
                asset->header.last_access = time(nullptr);
                return asset;
            }
        }
    }

    if (asset == nullptr) {
        llw("Could not find font %s (font size: %d), returning default font %s (font size: %d)", name, font_size, A_DEFAULT_FONT, A_DEFAULT_FONT_SIZE);
        return asset_get_font(A_DEFAULT_FONT, A_DEFAULT_FONT_SIZE);
    }

    F32 const start_time = time_get_glfw();
    i_load_font(asset->header.path, font_size);
    F32 const end_time = time_get_glfw();
    if (end_time - start_time > A_MAX_LOAD_TIME_BEFORE_WARNING) {
        llw("Loading font %s took %.2f seconds and is above the threshold of %.2f seconds.", name, end_time - start_time, A_MAX_LOAD_TIME_BEFORE_WARNING);
    }

    return asset_get_font(name, font_size);
}

ASkybox *asset_get_skybox(C8 const *name) {
    for (SZ i = 0; i < g_assets.skybox_count; ++i) {
        ASkybox *asset = &g_assets.skyboxes[i];
        if (ou_strcmp(asset->header.name, name) == 0) {
            asset->header.last_access = time(nullptr);
            return asset;
        }
    }

    F32 const start_time = time_get_glfw();
    i_load_skybox(TS("%s/%s", A_SKYBOXES_PATH, name)->c);
    F32 const end_time = time_get_glfw();
    if (end_time - start_time > A_MAX_LOAD_TIME_BEFORE_WARNING) {
        llw("Loading skybox %s took %.2f seconds and is above the threshold of %.2f seconds.", name, end_time - start_time, A_MAX_LOAD_TIME_BEFORE_WARNING);
    }

    return asset_get_skybox(name);
}

ATerrain *asset_get_terrain(C8 const *name, Vector3 dimensions) {
    for (SZ i = 0; i < g_assets.terrain_count; ++i) {
        ATerrain *asset            = &g_assets.terrains[i];
        BOOL const same_name       = ou_strcmp(asset->header.name, name) == 0;
        BOOL const same_dimensions = Vector3Equals(asset->dimensions, dimensions);
        if (same_name && same_dimensions) {
            asset->header.last_access = time(nullptr);
            return asset;
        }
    }

    F32 const start_time = time_get_glfw();
    i_load_terrain(TS("%s/%s", A_TERRAINS_PATH, name)->c, dimensions);
    F32 const end_time = time_get_glfw();
    if (end_time - start_time > A_MAX_LOAD_TIME_BEFORE_WARNING) {
        llw("Loading terrain %s took %.2f seconds and is above the threshold of %.2f seconds.", name, end_time - start_time, A_MAX_LOAD_TIME_BEFORE_WARNING);
    }

    return asset_get_terrain(name, dimensions);
}

void asset_set_model_shader(AModel *model, Shader shader) {
    for (S32 i = 0; i < model->base.materialCount; ++i) { model->base.materials[i].shader = shader; }
}

void asset_set_model_shader_rl(Model *model, Shader shader) {
    for (S32 i = 0; i < model->materialCount; ++i) { model->materials[i].shader = shader; }
}

void asset_set_skybox_shader(ASkybox *skybox, Shader shader) {
    skybox->model.materials[0].shader = shader;
}

void asset_set_terrain_shader(ATerrain *terrain, Shader shader) {
    terrain->model.materials[0].shader = shader;
}

void static i_dump(void *asset) {
    auto *header = (AHeader *)asset;

    C8 pretty_size[PRETTY_BUFFER_SIZE] = {};
    unit_to_pretty_prefix_binary_u("B", header->file_size, pretty_size, PRETTY_BUFFER_SIZE, UNIT_PREFIX_BINARY_MEBI);

    time_t const current_unix_time = time(nullptr);
    F64 const ns_since_access      = header->last_access > 0 ? BASE_TO_NANO(difftime(current_unix_time, header->last_access)) : 0.0;
    F64 const ns_since_modified    = header->last_modified > 0 ? BASE_TO_NANO(difftime(current_unix_time, header->last_modified)) : 0.0;
    C8 pretty_access[PRETTY_BUFFER_SIZE] = {};
    C8 pretty_modified[PRETTY_BUFFER_SIZE] = {};

    if (header->last_access > 0) {
        unit_to_pretty_time_f(ns_since_access, pretty_access, PRETTY_BUFFER_SIZE, UNIT_TIME_DAYS);
    } else {
        ou_snprintf(pretty_access, PRETTY_BUFFER_SIZE, "never");
    }

    if (header->last_modified > 0) {
        unit_to_pretty_time_f(ns_since_modified, pretty_modified, PRETTY_BUFFER_SIZE, UNIT_TIME_DAYS);
    } else {
        ou_snprintf(pretty_modified, PRETTY_BUFFER_SIZE, "unknown");
    }

    String *common = TS("\\ouc{#c0d6e4ff}%-40s \\ouc{#a0a0a0ff}%-65s \\ouc{#ffd700ff}%12s \\ouc{#00ff7fff}%12s \\ouc{#ff69b4ff}%12s", header->name,
                        header->path, pretty_access, pretty_modified, pretty_size);

    switch (header->type) {
        case A_TYPE_MODEL: {
            auto *model = (AModel *)asset;
            String *details = TS("  \\ouc{#ff5e48ff}Verts: \\ouc{#ffffffff}%zu\\ouc{#ff5e48ff}, BBox: \\ouc{#ffffffff}(%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f)",
                                 model->vertex_count, model->bb.min.x, model->bb.min.y, model->bb.min.z, model->bb.max.x, model->bb.max.y, model->bb.max.z);
            lln("%s%s", common->c, details->c);
        } break;
        case A_TYPE_FONT: {
            auto *font = (AFont *)asset;
            if (font->font_size == 0 && !font->header.loaded) { return; }  // Skip unloaded default size fonts
            String *details = TS("  \\ouc{#ff5e48ff}Size: \\ouc{#ffffffff}%d", font->font_size);
            lln("%s%s", common->c, details->c);
        } break;
        case A_TYPE_TEXTURE: {
            auto *texture = (ATexture *)asset;
            String *details = TS("  \\ouc{#ff5e48ff}Size: \\ouc{#ffffffff}%dx%d", texture->base.width, texture->base.height);
            lln("%s%s", common->c, details->c);
        } break;
        case A_TYPE_SOUND: {
            auto *sound = (ASound *)asset;
            String *details = TS("  \\ouc{#ff5e48ff}Len: \\ouc{#ffffffff}%u ms", sound->length);
            lln("%s%s", common->c, details->c);
        } break;
        case A_TYPE_SHADER: {
            auto *shader = (AShader *)asset;
            String *details = TS("  \\ouc{#ff5e48ff}ID: \\ouc{#ffffffff}%d", shader->base.id);
            lln("%s%s", common->c, details->c);
        } break;
        case A_TYPE_SKYBOX: {
            auto *skybox = (ASkybox *)asset;
            String *details =
                TS("  \\ouc{#ff5e48ff}Refs: \\ouc{#ffffffff}tex=%s, shader=%s, cubemap=%s",
                   skybox->texture ? skybox->texture->header.name : "null",
                   skybox->skybox_shader ? skybox->skybox_shader->header.name : "null",
                   skybox->cubemap_shader ? skybox->cubemap_shader->header.name : "null");
            lln("%s%s", common->c, details->c);
        } break;
        case A_TYPE_TERRAIN: {
            auto *terrain = (ATerrain *)asset;
            String *details =
                TS("  \\ouc{#ff5e48ff}Dims: \\ouc{#ffffffff}%.2f x %.2f x %.2f\\ouc{#ff5e48ff}, Refs: \\ouc{#ffffffff}height=%s, diffuse=%s",
                   terrain->dimensions.x, terrain->dimensions.y, terrain->dimensions.z,
                   terrain->height_texture ? terrain->height_texture->header.name : "null",
                   terrain->diffuse_texture ? terrain->diffuse_texture->header.name : "null");
            lln("%s%s", common->c, details->c);
        } break;
        default: {
            _unreachable_();
        } break;
    }
}

void asset_print_state() {
    console_draw_separator(); for (SZ i = 0; i < g_assets.model_count; ++i)   { i_dump(&g_assets.models[i]);   }
    console_draw_separator(); for (SZ i = 0; i < g_assets.texture_count; ++i) { i_dump(&g_assets.textures[i]); }
    console_draw_separator(); for (SZ i = 0; i < g_assets.sound_count; ++i)   { i_dump(&g_assets.sounds[i]);   }
    console_draw_separator(); for (SZ i = 0; i < g_assets.shader_count; ++i)  { i_dump(&g_assets.shaders[i]);  }
    console_draw_separator(); for (SZ i = 0; i < g_assets.font_count; ++i)    { i_dump(&g_assets.fonts[i]);    }
    console_draw_separator(); for (SZ i = 0; i < g_assets.skybox_count; ++i)  { i_dump(&g_assets.skyboxes[i]); }
    console_draw_separator(); for (SZ i = 0; i < g_assets.terrain_count; ++i) { i_dump(&g_assets.terrains[i]); }
}

C8 const *asset_type_to_cstr(AType type) {
    return i_assets_type_to_cstr[type];
}

F32 asset_get_animation_duration(C8 const *model_name, U32 anim_index, F32 fps, F32 anim_speed) {
    AModel *model = asset_get_model(model_name);
    if (!model)                                    { return 0.0F; }
    if (!model->has_animations)                    { return 0.0F; }
    if (anim_index >= (U32)model->animation_count) { return 0.0F; }

    ModelAnimation const &anim = model->animations[anim_index];
    F32 const frame_duration      = 1.0F / fps;
    F32 const base_duration       = (F32)anim.frameCount * frame_duration;
    F32 const real_world_duration = base_duration / anim_speed;

    return real_world_duration;
}

String static *i_ablob_get_version(U8 *data) {
    ABlobHeader header = {};
    ou_memcpy(&header, data, sizeof(ABlobHeader));
    return TS("%u", header.version);
}

String static *i_ablob_get_created(U8 *data) {
    ABlobHeader header = {};
    ou_memcpy(&header, data, sizeof(ABlobHeader));

    auto timestamp     = (time_t)header.created;
    struct tm *tm_info = localtime(&timestamp);
    if (!tm_info) { return TS("unknown"); }

    C8 time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    return TS("%s", time_str);
}

void asset_blob_init() {
    if (!FileExists(A_BLOB_FILE_PATH) || args_get_bool("RebuildBlob")) {
        asset_blob_write();
    }
    asset_blob_read();
}

void asset_blob_write() {
    asset_blob_unload();

    FilePathList const list = LoadDirectoryFilesEx(A_ASSETS_PATH, nullptr, true);
    if (list.count == 0) { lle("Could not find any files in folder '%s'", A_ASSETS_PATH); }

    // Reuse existing array instead of allocating new one
    if (g_assets.blobs.data == nullptr) {
        array_init(MEMORY_TYPE_PARENA, &g_assets.blobs, list.count);
    } else {
        array_clear(&g_assets.blobs);
    }
    SZ total_size = SZ_MIN;

    for (SZ i = 0; i < list.count; ++i) {
        ABlob blob     = {};
        S32 size       = S32_MIN;
        ou_strncpy(blob.filepath, list.paths[i], A_PATH_MAX_LENGTH);
        blob.data      = LoadFileData(blob.filepath, &size);
        blob.size      = (SZ)size;

        if (blob.size == 0) { lle("Could not load file '%s'", blob.filepath); }

        array_push(&g_assets.blobs, blob);
        total_size += blob.size;
    }

    // Header
    total_size += sizeof(ABlobHeader);

    U8 *write_data = (U8*)memory_oucalloc(1, total_size, MEMORY_TYPE_TARENA);
    SZ  write_size = 0;

    // Write header
    ABlobHeader header = {};
    header.version = A_BLOB_VERSION;
    header.created = (U64)time(nullptr);

    ou_memcpy(write_data + write_size, &header, sizeof(ABlobHeader));
    write_size += sizeof(ABlobHeader);

    // Write asset data
    for (SZ i = 0; i < g_assets.blobs.count; ++i) {
        ABlob* b = &g_assets.blobs.data[i];
        ou_memcpy(write_data + write_size, b->data, b->size);
        write_size += b->size;
    }

    if (!SaveFileData(A_BLOB_FILE_PATH, write_data, (S32)write_size)) { lle("Failed to save '%s'", A_BLOB_FILE_PATH); }

    UnloadDirectoryFiles(list);

    String *version = i_ablob_get_version(write_data);
    String *created = i_ablob_get_created(write_data);

    lld("Asset blob written %.2f MB (version: %s, created: %s)", ((F32)write_size)/1024.0F/1024.0F, version->c, created->c);
}

void asset_blob_read() {
    S32 read_size = S32_MIN;
    U8 *read_data = LoadFileData(A_BLOB_FILE_PATH, &read_size);
    if (!read_data)                           { lle("Failed to load '%s'", A_BLOB_FILE_PATH);                     return; }
    if (read_size < (S32)sizeof(ABlobHeader)) { lle("Blob file too small for header"); UnloadFileData(read_data); return; }

    String *version = i_ablob_get_version(read_data);
    String *created = i_ablob_get_created(read_data);

    lld("Asset blob read %.2f MB (version: %s, created: %s)", ((F32)read_size)/1024.0F/1024.0F, version->c, created->c);

    UnloadFileData(read_data);
}

void asset_blob_unload() {
    for (SZ i = 0; i < g_assets.blobs.count; ++i) {
        ABlob *blob = &g_assets.blobs.data[i];
        if (blob->data) {
            UnloadFileData(blob->data);
            blob->data = nullptr;
        }
        blob->filepath[0] = '\0';
        blob->size = 0;
    }

    lld("Asset blob unloaded (%zu blobs)", g_assets.blobs.count);
    array_clear(&g_assets.blobs);
}
