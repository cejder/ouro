#include "console.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "audio.hpp"
#include "color.hpp"
#include "core.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "entity.hpp"
#include "input.hpp"
#include "log.hpp"
#include "math.hpp"
#include "memory.hpp"
#include "message.hpp"
#include "profiler.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "std.hpp"
#include "string.hpp"
#include "test.hpp"
#include "time.hpp"
#include "unit.hpp"
#include "world.hpp"

#include <raymath.h>
#include <algorithm>

#define CONSOLE_LINE_COUNT_MIN 5
#define CONSOLE_SCROLLBAR_COLOR Fade(SOFTLAVENDER, 0.95F)
#define CONSOLE_INPUT_CURSOR_COLOR SUNSETAMBER
#define CONSOLE_PREDICTION_FOREGROUND_COLOR EVAFATORANGE
#define CONSOLE_INPUT_BACKGROUND_COLOR Fade(DEEPOCEANBLUE, 0.95F)
#define CONSOLE_INPUT_FOREGROUND_COLOR FROSTWHITE
#define CONSOLE_OUTPUT_BACKGROUND_COLOR Fade(NAYGREEN, 0.95F)
#define CONSOLE_OUTPUT_FOREGROUND_COLOR NAYBEIGE

Console g_console = {};

#define con_cmd_decl(name) BOOL static con_cmd_##name(ConCMD const *cmd)

con_cmd_decl(cvars);
con_cmd_decl(clear);
con_cmd_decl(dt_mod);
con_cmd_decl(dump);
con_cmd_decl(echo);
con_cmd_decl(e_goto);
con_cmd_decl(e_goto_name);
con_cmd_decl(e_list);
con_cmd_decl(e_position);
con_cmd_decl(e_rotate);
con_cmd_decl(e_scale);
con_cmd_decl(e_model);
con_cmd_decl(a_play);
con_cmd_decl(cam_to_cb);
con_cmd_decl(exit);
con_cmd_decl(help);
con_cmd_decl(light_goto);
con_cmd_decl(list);
con_cmd_decl(scene);
con_cmd_decl(shell);
con_cmd_decl(teleport);
con_cmd_decl(test);
con_cmd_decl(rebuild_blob);
con_cmd_decl(history);

enum ConCMDType : U8 {
    CON_CMD_TYPE_CVARS,
    CON_CMD_TYPE_CLEAR,
    CON_CMD_TYPE_DT_MODIFIER,
    CON_CMD_TYPE_DUMP,
    CON_CMD_TYPE_ECHO,
    CON_CMD_TYPE_E_GOTO,
    CON_CMD_TYPE_E_GOTO_NAME,
    CON_CMD_TYPE_E_LIST,
    CON_CMD_TYPE_E_POSITION,
    CON_CMD_TYPE_E_ROTATE,
    CON_CMD_TYPE_E_SCALE,
    CON_CMD_TYPE_E_MODEL,
    CON_CMD_TYPE_A_PLAY,
    CON_CMD_TYPE_CAM_TO_CB,
    CON_CMD_TYPE_EXIT,
    CON_CMD_TYPE_HELP,
    CON_CMD_TYPE_LIGHT_GOTO,
    CON_CMD_TYPE_LIST,
    CON_CMD_TYPE_SCENE,
    CON_CMD_TYPE_SHELL,
    CON_CMD_TYPE_TELEPORT,
    CON_CMD_TYPE_TEST,
    CON_CMD_TYPE_REBUILD_BLOB,
    CON_CMD_TYPE_HISTORY,
    CON_CMD_TYPE_COUNT,
};

using ConCMDFunc = BOOL (*)(ConCMD const *);

struct ConCMD {
    C8 const *name;
    C8 const *desc;
    C8 const *example;
    ConCMDType type;
    ConCMDFunc func;
    C8 *args[CON_CMD_ARGS_MAX];
    void *data;
} static i_cmds[CON_CMD_TYPE_COUNT] = {
    { "cvars",                "List all console variables",                                      "cvars",                                   CON_CMD_TYPE_CVARS,         con_cmd_cvars         },
    { "clear",                "Clears the console output",                                       "clear",                                   CON_CMD_TYPE_CLEAR,         con_cmd_clear         },
    { "dtm",                   "Set the delta time modifier",                                     "dtm {value}",                            CON_CMD_TYPE_DT_MODIFIER,   con_cmd_dt_mod        },
    { "dump",                 "Dumps some information about the current state",                  "dump",                                    CON_CMD_TYPE_DUMP,          con_cmd_dump          },
    { "echo",                 "Prints a message with a given level",                             "echo {abbr_log_lvl, abbr_msg_lvl} {msg}", CON_CMD_TYPE_ECHO,          con_cmd_echo          },
    { "e_goto",               "Moves the camera near the entity and looks at it",                "e_goto {ent_id}",                         CON_CMD_TYPE_E_GOTO,        con_cmd_e_goto        },
    { "e_goto_name",          "Moves the camera near the entity by name and looks at it",        "e_goto_name {ent_name}",                  CON_CMD_TYPE_E_GOTO_NAME,   con_cmd_e_goto_name   },
    { "e_list",               "Lists all entities with id, name, position, scale, and rotation", "e_list",                                  CON_CMD_TYPE_E_LIST,        con_cmd_e_list        },
    { "e_position",           "Sets the position of an entity",                                  "e_position {ent_id} {x} {y} {z}",         CON_CMD_TYPE_E_POSITION,    con_cmd_e_position    },
    { "e_rotate",             "Sets the rotation of an entity",                                  "e_rotate {ent_id} {r}",                   CON_CMD_TYPE_E_ROTATE,      con_cmd_e_rotate      },
    { "e_scale",              "Sets the scale of an entity",                                     "e_scale {ent_id} {x} {y} {z}",            CON_CMD_TYPE_E_SCALE,       con_cmd_e_scale       },
    { "e_model",              "Sets the model of an entity",                                     "e_model {ent_id} {name}",                 CON_CMD_TYPE_E_MODEL,       con_cmd_e_model       },
    { "a_play",               "Play an audio file on a given channel",                           "a_play {channel} {name}",                 CON_CMD_TYPE_A_PLAY,        con_cmd_a_play        },
    { "cam_to_cb",            "Copy the current camera info to the clipboard",                   "cam_to_cb",                               CON_CMD_TYPE_CAM_TO_CB,     con_cmd_cam_to_cb     },
    { "exit",                 "Exits the game",                                                  "exit",                                    CON_CMD_TYPE_EXIT,          con_cmd_exit          },
    { "help",                 "Shows all available cmds",                                        "help",                                    CON_CMD_TYPE_HELP,          con_cmd_help          },
    { "light_goto",           "Moves the camera near the light and looks at it",                 "light_goto {light_idx}",                  CON_CMD_TYPE_LIGHT_GOTO,    con_cmd_light_goto    },
    { "list",                 "Lists a resource type (e.g. scenes)",                             "list {resource_name}",                    CON_CMD_TYPE_LIST,          con_cmd_list          },
    { "scene",                "Set/get the current scene",                                       "scene {set, get} {scene_name}",           CON_CMD_TYPE_SCENE,         con_cmd_scene         },
    { "s",                    "Executes a shell commmand",                                       "s {cmd}",                                 CON_CMD_TYPE_SHELL,         con_cmd_shell         },
    { "teleport",             "Moves the camera near the position and looks at it",              "teleport {x} {y} {z}",                    CON_CMD_TYPE_TELEPORT,      con_cmd_teleport      },
    { "test",                 "Runs all tests n times",                                          "test {count}",                            CON_CMD_TYPE_TEST,          con_cmd_test          },
    { "rebuild_blob",         "Force recreation of the asset blob file",                         "rebuild_blob",                            CON_CMD_TYPE_REBUILD_BLOB,  con_cmd_rebuild_blob  },
    { "history",              "Shows command history",                                           "history",                                 CON_CMD_TYPE_HISTORY,       con_cmd_history       },
};

void static i_print_help_for_cmd(ConCMD *cmd) {
    console_draw_separator();
    lln("Name:        %s", cmd->name);
    lln("Description: %s", cmd->desc);
    lln("Example:     %s", cmd->example);
}

BOOL con_cmd_help(ConCMD const *cmd) {
    if (cmd->args[0] != nullptr) {
        llw("Command help does not take any arguments");
        return false;
    }

    lln("Available commands:");
    for (auto & it : i_cmds) { i_print_help_for_cmd(&it); }

    return true;
}

struct IConsoleRenderInfo {
    Vector2 position;
    Vector2 size;
    AFont *font;
};

struct IConsoleColorPair {
    Color color_a;
    Color color_b;
} static i_output_colors_for_prefix[] = {
    {CONSOLE_OUTPUT_FOREGROUND_COLOR, WHITE},  // NONE.
    {SKYBLUE, BLUE},                           // TRACE.
    {GOLD, YELLOW},                            // DEBUG.
    {GREEN, LIME},                             // INFO.
    {ORANGE, SALMON},                          // WARN.
    {RED, MAROON},                             // ERROR.
    {MAGENTA, PURPLE},                         // FATAL.
};

void static i_list_cvars() {
    // Create header and entry format strings
    C8 header_format[256];
    C8 entry_format[256];
    C8 divider_format[256];

    // Header with clean, professional formatting
    ou_snprintf(header_format, sizeof(header_format),
                "\\ouc{#ff3300ff}%%-%ds \\ouc{#ff3300ff}%%-%ds   \\ouc{#ff3300ff}%%-%ds    \\ouc{#ff3300ff}%%s",
                CVAR_LONGEST_NAME_LENGTH, CVAR_LONGEST_VALUE_LENGTH, CVAR_LONGEST_TYPE_LENGTH);

    // Entry format with delimiters and comment support
    ou_snprintf(entry_format, sizeof(entry_format),
                "\\ouc{#ffcc00ff}%%-%ds \\ouc{#aaaaffff}%%-%ds   \\ouc{#ffaaaaff}%%-%ds    \\ouc{#00ff00ff}%%s",
                CVAR_LONGEST_NAME_LENGTH, CVAR_LONGEST_VALUE_LENGTH, CVAR_LONGEST_TYPE_LENGTH);

    // Simple divider
    ou_snprintf(divider_format, sizeof(divider_format), "\\ouc{#ff3300ff}%%s");

    // Print header
    lln("Available CVars (%d):", CVAR_COUNT);

    // Print column headers
    lln(header_format, "NAME", "VALUE", "TYPE", "COMMENT");

    // List all CVars with their current values and types (using sorted indices)
    for (const auto &cvar : cvar_meta_table) {
        C8 value_str[64];
        C8 const *type_str = nullptr;

        switch (cvar.type) {
            case CVAR_TYPE_BOOL: {
                ou_strncpy(value_str, BOOL_TO_STR(*(BOOL *)cvar.address), sizeof(value_str));
                type_str = "BOOL";
            } break;
            case CVAR_TYPE_S32: {
                ou_snprintf(value_str, sizeof(value_str), "%d", *(S32 *)cvar.address);
                type_str = "S32";
            } break;
            case CVAR_TYPE_F32: {
                ou_snprintf(value_str, sizeof(value_str), "%.8f", *(F32 *)cvar.address);
                type_str = "F32";
            } break;
            case CVAR_TYPE_CVARSTR: {
                ou_strncpy(value_str, (*(CVarStr *)cvar.address).cstr, sizeof(value_str) - 1);
                value_str[sizeof(value_str) - 1] = '\0';
                type_str = "CVarStr";
            } break;
            default: {
                _unreachable_();
            }
        }

        lln(entry_format, cvar.name, value_str, type_str, cvar.comment[0] != '\0' ? cvar.comment : "");
    }
}

BOOL con_cmd_cvars(ConCMD const *cmd) {
    if (cmd->args[0] != nullptr) {
        llw("command cvars does not take any arguments");
        return false;
    }

    i_list_cvars();

    return true;
}

BOOL con_cmd_test(ConCMD const *cmd) {
    // If it is not specified, we default to 1.
    U32 count     = 1;
    C8 *count_str = cmd->args[0];
    if (count_str) {
        if (ou_sscanf(count_str, "%u", &count) != 1) {
            llw("Could not parse count as U32: %s", count_str);
            return false;
        }
    }

    for (SZ i = 0; i < count; ++i) {
        F32 const start_time   = time_get_glfw();
        BOOL const success     = test_run();
        F32 const end_time     = time_get_glfw();
        F32 const elapsed_time = end_time - start_time;
        lln("Test_%zu_of_%u: %s in %f ms", i + 1, count, success ? "Success" : "Failure", elapsed_time);
    }

    C8 pretty_buffer_used[PRETTY_BUFFER_SIZE] = {};
    C8 pretty_buffer_max[PRETTY_BUFFER_SIZE]  = {};

    // We also print the current memory statistics.
    for (S32 i = MEMORY_TYPE_PARENA; i < MEMORY_TYPE_COUNT; ++i) {
        MemoryArenaStats const stats = memory_get_current_arena_stats((MemoryType)i);
        unit_to_pretty_prefix_binary_u("B", stats.total_used, pretty_buffer_used, PRETTY_BUFFER_SIZE, UNIT_PREFIX_BINARY_MEBI);
        unit_to_pretty_prefix_binary_u("B", stats.max_used, pretty_buffer_max, PRETTY_BUFFER_SIZE, UNIT_PREFIX_BINARY_MEBI);
        lln("Memory_%s: %zu arenas, %zu allocations, %s used, %s max",
            memory_type_to_cstr((MemoryType)i),
            stats.arena_count, stats.total_allocation_count,
            pretty_buffer_used, pretty_buffer_max);
    }

    return true;
}

BOOL con_cmd_rebuild_blob(ConCMD const *cmd) {
    unused(cmd);

    asset_blob_write();

    lln("Asset blob rebuilt and written to disk (not reloaded in memory)");

    return true;
}

BOOL con_cmd_scene(ConCMD const *cmd) {
    C8 *set_or_get = cmd->args[0];
    if (!set_or_get) {
        llw("Could not parse set or get");
        return false;
    }

    if (ou_strcmp(set_or_get, "set") == 0) {
        C8 *scene = cmd->args[1];
        if (!scene) {
            llw("Could not parse scene");
            return false;
        }

        SceneType const t = scenes_set_scene_by_name(scene);
        lln("Set_Scene: %s", scenes_to_cstr(t));

        return true;
    }

    if (ou_strcmp(set_or_get, "get") == 0) {
        lln("Current_Scene: %s", scenes_to_cstr(g_scenes.current_scene_type));
        return true;
    }

    llw("Unknown set or get: %s", set_or_get);

    return false;
}

BOOL con_cmd_dump(ConCMD const *cmd) {
    unused(cmd);

    asset_print_state();

    console_draw_separator();
    lln("Player_Position: %.2f %.2f %.2f", g_world->player.camera3d.position.x, g_world->player.camera3d.position.y, g_world->player.camera3d.position.z);
    lln("Player_Target: %.2f %.2f %.2f", g_world->player.camera3d.target.x, g_world->player.camera3d.target.y, g_world->player.camera3d.target.z);

    if (g_world->selected_id != INVALID_EID) {
        console_draw_separator();
        Vector3 const position = g_world->position[g_world->selected_id];
        Vector3 const scale    = g_world->scale[g_world->selected_id];
        F32 const rotation     = g_world->rotation[g_world->selected_id];
        lln("Target_Position: %.2f %.2f %.2f", position.x, position.y, position.z);
        lln("Target_Scale: %.2f %.2f %.2f", scale.x, scale.y, scale.z);
        lln("Target_Rotation: %.2f", rotation);
    }
    console_draw_separator();

    lighting_dump();

    return true;
}

BOOL con_cmd_dt_mod(ConCMD const *cmd) {
    C8 const *modifier = cmd->args[0];
    if (!modifier) {
        llw("Could not parse modifier");
        return false;
    }

    F32 value = 0.0F;
    if (ou_sscanf(modifier, "%f", &value) == 1) {
        time_set_delta_mod(value);
    } else {
        llw("Could not parse modifier as a floating-point number");
        return false;
    }

    time_set_delta_mod(value);
    lln("Set_Delta_Modifier: %f", value);

    return true;
}

void static i_print_header(AHeader *header) {
    lln("%s:", header->name);
    lln("  Path: %s", header->path);
    lln("  Last Access: %s", ctime(&header->last_access));
    lln("  Last Modified: %s", ctime(&header->last_modified));

    switch (header->type) {
        case A_TYPE_MODEL: {
            auto *model     = (AModel *)header;
            BoundingBox *bb = &model->bb;
            lln("  Bounding Box: (%f, %f, %f) - (%f, %f, %f)", bb->min.x, bb->min.y, bb->min.z, bb->max.x, bb->max.y, bb->max.z);
            lln("  Vertex Count: %zu", model->vertex_count);
        } break;

        case A_TYPE_TEXTURE: {
            auto *texture = (ATexture *)header;
            Image *image  = &texture->image;
            lln("  Dimensions: %d x %d", image->width, image->height);
            lln("  Mipmaps: %d", image->mipmaps);
            lln("  Format: %d", image->format);
        } break;

        case A_TYPE_SOUND: {
            SZ const buffer_size = 256;
            C8 pretty_buffer[buffer_size];

            auto const *sound      = (ASound *)header;
            F32 const length_in_ns = (F32)BASE_TO_NANO(MILLI_TO_BASE(sound->length));
            unit_to_pretty_time_f(length_in_ns, pretty_buffer, buffer_size, UNIT_TIME_MINUTES);
            lln("  Length: %s", pretty_buffer);
        } break;

        case A_TYPE_SHADER: {
            C8 const *vpath = TS("%s/vert.glsl", header->path)->c;
            C8 const *fpath = TS("%s/frag.glsl", header->path)->c;
            if (FileExists(vpath)) { lln("  Vertex Path: %s", vpath); }
            if (FileExists(fpath)) { lln("  Fragment Path: %s", fpath); }
        } break;

        case A_TYPE_FONT: {
            auto *font = (AFont *)header;
            lln("  Size: %d", font->font_size);
        } break;

        case A_TYPE_SKYBOX: {
            auto *skybox = (ASkybox *)header;
            lln("  Texture: %s", skybox->texture->header.name);
            lln("  Shader (Skybox): %s", skybox->skybox_shader->header.name);
            lln("  Shader (Cubemap): %s", skybox->cubemap_shader->header.name);
        } break;

        case A_TYPE_TERRAIN: {
            auto *terrain = (ATerrain *)header;
            lln("  Dimensions: %.0f x %.0f", terrain->dimensions.x, terrain->dimensions.y);
            lln("  Texture (Height): %s", terrain->height_texture->header.name);
            lln("  Texture (Diffused): %s", terrain->diffuse_texture->header.name);
        } break;

        default: {
            _unreachable_();
        } break;
    }
}

BOOL con_cmd_list(ConCMD const *cmd) {
    // List all resources if no resource name is given or if the resource name is "resources".
    C8 *resource_name = cmd->args[0];
    if (!resource_name || ou_strcmp(resource_name, "resources") == 0) {
        lln("Available_Resources:");
        lln("- cvars");
        lln("- scenes");
        lln("- models");
        lln("- textures");
        lln("- sounds");
        lln("- shaders");
        lln("- fonts");
        lln("- skyboxes");
        lln("- terrains");
        return true;
    }

    if (ou_strcmp(resource_name, "cvars") == 0) {
        i_list_cvars();
        return true;
    }

    if (ou_strcmp(resource_name, "scenes") == 0) {
        lln("Available_Scenes:");
        for (S32 i = 1; i < NUMBER_OF_SCENES + 1; ++i) {
            if (i == g_scenes.current_scene_type) {
                lln("- %s (current)", scenes_to_cstr((SceneType)i));
                continue;
            }
            if (i == g_scenes.current_overlay_scene_type) {
                lln("- %s (overlay)", scenes_to_cstr((SceneType)i));
                continue;
            }
            lln("- %s", scenes_to_cstr((SceneType)i));
        }
        return true;
    }

    if (ou_strcmp(resource_name, "models") == 0) {
        lln("Available_Models:");
        for (SZ i = 0; i < g_assets.model_count; ++i) { i_print_header(&g_assets.models[i].header); }
        return true;
    }

    if (ou_strcmp(resource_name, "textures") == 0) {
        lln("Available_Textures:");
        for (SZ i = 0; i < g_assets.texture_count; ++i) { i_print_header(&g_assets.textures[i].header); }
        return true;
    }

    if (ou_strcmp(resource_name, "sounds") == 0) {
        lln("Available_Sounds:");
        for (SZ i = 0; i < g_assets.sound_count; ++i) { i_print_header(&g_assets.sounds[i].header); }
        return true;
    }

    if (ou_strcmp(resource_name, "shaders") == 0) {
        lln("Available_Shaders:");
        for (SZ i = 0; i < g_assets.shader_count; ++i) { i_print_header(&g_assets.shaders[i].header); }
        return true;
    }

    if (ou_strcmp(resource_name, "fonts") == 0) {
        lln("Available_Fonts:");
        for (SZ i = 0; i < g_assets.font_count; ++i) { i_print_header(&g_assets.fonts[i].header); }
        return true;
    }

    if (ou_strcmp(resource_name, "skyboxes") == 0) {
        lln("Available_Skyboxes:");
        for (SZ i = 0; i < g_assets.skybox_count; ++i) { i_print_header(&g_assets.skyboxes[i].header); }
        return true;
    }

    if (ou_strcmp(resource_name, "terrains") == 0) {
        lln("Available_Terrains:");
        for (SZ i = 0; i < g_assets.terrain_count; ++i) { i_print_header(&g_assets.terrains[i].header); }
        return true;
    }

    llw("Unknown resource name: %s", resource_name);

    return false;
}

BOOL con_cmd_teleport(ConCMD const *cmd) {
    Camera3D *camera = c3d_get_ptr();
    Vector3 position = {};

    C8 *str = cmd->args[0];
    if (!str) {
        llw("Could not parse coordinate x");
        return false;
    }
    if (ou_strcmp(str, "s") == 0) {
        position.x = camera->position.x;
    } else {
        if (ou_sscanf(str, "%f", &position.x) != 1) {
            llw("Could not parse x coordinate as a floating-point number: %s", str);
            return false;
        }
    }

    str = cmd->args[1];
    if (!str) {
        llw("Could not parse coordinate y");
        return false;
    }
    if (ou_strcmp(str, "s") == 0) {
        position.y = camera->position.y;
    } else {
        if (ou_sscanf(str, "%f", &position.y) != 1) {
            llw("Could not parse y coordinate as a floating-point number: %s", str);
            return false;
        }
    }

    str = cmd->args[2];
    if (!str) {
        llw("Could not parse coordinate z");
        return false;
    }
    if (ou_strcmp(str, "s") == 0) {
        position.z = camera->position.z;
    } else {
        if (ou_sscanf(str, "%f", &position.z) != 1) {
            llw("Could not parse z coordinate as a floating-point number: %s", str);
            return false;
        }
    }

    camera->target   = position;
    camera->position = (Vector3AddValue(position, 0.5F));

    lln("Teleported to position (%f, %f, %f)", position.x, position.y, position.z);

    return true;
}

void static i_goto_light(SZ idx) {
    Vector3 const position = g_lighting.lights[idx].position;
    Camera3D *camera       = c3d_get_ptr();
    camera->target         = position;
    camera->position       = (Vector3AddValue(position, 2.5F));

    lln("Light with index %zu found at position (%f, %f, %f)", idx, position.x, position.y, position.z);
}

BOOL con_cmd_light_goto(ConCMD const *cmd) {
    SZ idx = 0;

    C8 *str = cmd->args[0];
    if (!str) {
        llw("Could not parse light index");
        return false;
    }
    if (ou_sscanf(str, "%zu", &idx) != 1) {
        llw("Could not parse light index as an integer: %s", str);
        return false;
    }

    if (idx >= LIGHTS_MAX) {
        llw("Light with index %zu does not exist. Currently there are only %zu lights ergo max index is %zu", idx, (SZ)LIGHTS_MAX, (SZ)(LIGHTS_MAX - 1));
        return false;
    }

    i_goto_light(idx);

    return true;
}

void static i_goto_ent(EID id) {
    Vector3 const position         = g_world->position[id];
    C8 *name                       = g_world->name[id];
    OrientedBoundingBox const *obb = &g_world->obb[id];

    // Get world space width (along world X axis) of a rotated OBB
    F32 const entity_width = math_abs_f32(obb->extents.x * obb->axes[0].x) + math_abs_f32(obb->extents.y * obb->axes[1].x) + math_abs_f32(obb->extents.z * obb->axes[2].x);

    Camera3D *camera = c3d_get_ptr();
    camera->target   = position;
    camera->position = (Vector3AddValue(position, entity_width * 2.0F));

    lln("Entity with name %s found at position (%f, %f, %f)", name, position.x, position.y, position.z);
}

BOOL con_cmd_e_goto(ConCMD const *cmd) {
    EID id = 0;

    C8 *str = cmd->args[0];
    if (!str) {
        llw("Could not parse entity id");
        return false;
    }
    if (ou_strcmp(str, "s") == 0) {
        id = g_world->selected_id;
    } else {
        if (ou_sscanf(str, "%d", &id) != 1) {
            llw("Could not parse entity id as an integer: %s", str);
            return false;
        }
    }

    if (!entity_is_valid(id)) {
        llw("Entity with id %d does not exist", id);
        return false;
    }

    i_goto_ent(id);

    return true;
}

BOOL con_cmd_e_goto_name(ConCMD const *cmd) {
    String *name = string_create_empty(MEMORY_TYPE_TARENA);

    for (SZ i = 0; cmd->args[i] != nullptr; ++i) {
        string_append(name, cmd->args[i]);
        string_append(name, " ");
    }
    string_trim_space(name);

    if (name->length == 0) {
        llw("Could not parse entity name");
        return false;
    }

    EID const id = entity_find_by_name(name->c);
    if (!entity_is_valid(id)) {
        llw("Entity with name %s does not exist", name->c);
        return false;
    }

    i_goto_ent(id);

    return true;
}

BOOL con_cmd_e_position(ConCMD const *cmd) {
    EID id = 0;

    C8 *str = cmd->args[0];
    if (!str) {
        llw("Could not parse entity id");
        return false;
    }
    if (ou_strcmp(str, "s") == 0) {
        id = g_world->selected_id;
    } else {
        if (ou_sscanf(str, "%d", &id) != 1) {
            llw("Could not parse entity id as an integer: %s", str);
            return false;
        }
    }

    if (!entity_is_valid(id)) {
        llw("Entity with id %d does not exist", id);
        return false;
    }

    Vector3 const old_position = g_world->position[id];
    Vector3 new_position       = {};

    str = cmd->args[1];
    if (!str) {
        llw("Could not parse coordinate x");
        return false;
    }
    if (ou_strcmp(str, "s") == 0) {
        new_position.x = old_position.x;
    } else {
        if (ou_sscanf(str, "%f", &new_position.x) != 1) {
            llw("Could not parse x coordinate as a floating-point number: %s", str);
            return false;
        }
    }

    str = cmd->args[2];
    if (!str) {
        llw("Could not parse coordinate y");
        return false;
    }
    if (ou_strcmp(str, "s") == 0) {
        new_position.y = old_position.y;
    } else {
        if (ou_sscanf(str, "%f", &new_position.y) != 1) {
            llw("Could not parse y coordinate as a floating-point number: %s", str);
            return false;
        }
    }

    str = cmd->args[3];
    if (!str) {
        llw("Could not parse coordinate z");
        return false;
    }
    if (ou_strcmp(str, "s") == 0) {
        new_position.z = old_position.z;
    } else {
        if (ou_sscanf(str, "%f", &new_position.z) != 1) {
            llw("Could not parse z coordinate as a floating-point number: %s", str);
            return false;
        }
    }

    entity_set_position(id, new_position);

    return true;
}

BOOL con_cmd_e_scale(ConCMD const *cmd) {
    EID id = 0;

    C8 *str = cmd->args[0];
    if (!str) {
        llw("Could not parse entity id");
        return false;
    }
    if (ou_strcmp(str, "s") == 0) {
        id = g_world->selected_id;
    } else {
        if (ou_sscanf(str, "%d", &id) != 1) {
            llw("Could not parse entity id as an integer: %s", str);
            return false;
        }
    }

    if (!entity_is_valid(id)) {
        llw("Entity with id %d does not exist", id);
        return false;
    }

    Vector3 const old_scale = g_world->scale[id];
    Vector3 new_scale       = {};

    str = cmd->args[1];
    if (!str) {
        llw("Could not parse scale x");
        return false;
    }
    if (ou_strcmp(str, "s") == 0) {
        new_scale.x = old_scale.x;
    } else {
        if (ou_sscanf(str, "%f", &new_scale.x) != 1) {
            llw("Could not parse x scale as a floating-point number: %s", str);
            return false;
        }
    }

    str = cmd->args[2];
    if (!str) {
        llw("Could not parse scale y");
        return false;
    }
    if (ou_strcmp(str, "s") == 0) {
        new_scale.y = old_scale.y;
    } else {
        if (ou_sscanf(str, "%f", &new_scale.y) != 1) {
            llw("Could not parse y scale as a floating-point number: %s", str);
            return false;
        }
    }

    str = cmd->args[3];
    if (!str) {
        llw("Could not parse scale z");
        return false;
    }
    if (ou_strcmp(str, "s") == 0) {
        new_scale.z = old_scale.z;
    } else {
        if (ou_sscanf(str, "%f", &new_scale.z) != 1) {
            llw("Could not parse z scale as a floating-point number: %s", str);
            return false;
        }
    }

    entity_set_scale(id, new_scale);

    return true;
}

BOOL con_cmd_e_model(ConCMD const *cmd) {
    EID id = 0;

    C8 *str = cmd->args[0];
    if (!str) {
        llw("Could not parse entity id");
        return false;
    }
    if (ou_strcmp(str, "s") == 0) {
        id = g_world->selected_id;
    } else {
        if (ou_sscanf(str, "%d", &id) != 1) {
            llw("Could not parse entity id as an integer: %s", str);
            return false;
        }
    }

    if (!entity_is_valid(id)) {
        llw("Entity with id %d does not exist", id);
        return false;
    }

    str = cmd->args[1];
    if (!str) {
        llw("Could not parse model name");
        return false;
    }

    entity_set_model(id, str);

    return true;
}

BOOL con_cmd_a_play(ConCMD const *cmd) {
    AudioChannelGroup acg = {};

    C8 const *acg_str = cmd->args[0];
    if (!acg_str) {
        llw("Could not parse audio channel group");
        return false;
    }

    if (ou_strcmp(acg_str, "music") == 0) {
        acg = ACG_MUSIC;
    } else if (ou_strcmp(acg_str, "sfx") == 0) {
        acg = ACG_SFX;
    } else if (ou_strcmp(acg_str, "ambience") == 0) {
        acg = ACG_AMBIENCE;
    } else if (ou_strcmp(acg_str, "voice") == 0) {
        acg = ACG_VOICE;
    } else {
        llw("Unknown audio channel group %s", acg_str);
        return false;
    }

    C8 const *name = cmd->args[1];
    if (!name) {
        llw("Could not parse sound name");
        return false;
    }

    C8 const *path = TS("%s/%s", A_SOUNDS_PATH, name)->c;
    if (!IsPathFile(path)) {
        llw("File '%s' does not exist", path);
        return false;
    }

    // NOTE:
    // This should not be necessary but who knows - maybe we will do some weird stuff
    // with the asset manager where some files are available in the file system but
    // won't be able to be loaded by the audio system... or something like that.k
    ASound const *sound = asset_get_sound(name);
    if (!sound) {
        llw("Could not find sound file with the path %s", path);
        return false;
    }

    audio_play(acg, name);

    return true;
}

BOOL con_cmd_e_rotate(ConCMD const *cmd) {
    EID id = 0;

    C8 *str = cmd->args[0];
    if (!str) {
        llw("Could not parse entity id");
        return false;
    }
    if (ou_strcmp(str, "s") == 0) {
        id = g_world->selected_id;
    } else {
        if (ou_sscanf(str, "%d", &id) != 1) {
            llw("Could not parse entity id as an integer: %s", str);
            return false;
        }
    }

    if (!entity_is_valid(id)) {
        llw("Entity with id %d does not exist", id);
        return false;
    }

    F32 const old_rotation = g_world->rotation[id];
    F32 new_rotation       = {};

    str = cmd->args[1];
    if (!str) {
        llw("Could not parse rotation x");
        return false;
    }
    if (ou_strcmp(str, "s") == 0) {
        new_rotation = old_rotation;
    } else {
        if (ou_sscanf(str, "%f", &new_rotation) != 1) {
            llw("Could not parse rotation as a floating-point number: %s", str);
            return false;
        }
    }

    entity_set_rotation(id, new_rotation);

    return true;
}

BOOL con_cmd_e_list(ConCMD const *cmd) {
    unused(cmd);

    world_dump_all_entities();

    return true;
}

void static i_strip_ansi_color_codes(C8 *str) {
    if (!str) { return; }

    C8 *read  = str;
    C8 *write = str;

    while (*read) {
        // Check for escape sequence start
        if (*read == '\033') {  // Same as *read == '\e'
            // Skip over the escape character
            read++;

            // If it's an ANSI color code (starts with '[')
            if (*read == '[') {
                read++;  // Skip the '['
                // Skip until we find a character that ends the sequence (a letter)
                while (*read && (*read < 'A' || (*read > 'Z' && *read < 'a') || *read > 'z')) { read++; }
                if (*read) { read++; }  // Skip the terminating character
                continue;
            }
        }
        // Copy the character
        *write++ = *read++;
    }

    // Null-terminate the string
    *write = '\0';
}

#ifndef _WIN32

#define MAX_SHELL_CMD_LENGTH 4096
#define MAX_SHELL_OUTPUT_LENGTH 4096

void static *i_execute_shell_cmd(void *arg) {
    C8 *shell_cmd = (C8 *)arg;
    FILE *fp      = nullptr;

    // Check if command starts with sudo
    if (ou_strncmp(shell_cmd, "sudo ", 5) == 0) {
        lln("\\ouc{#ff9900ff}Warning: Tried to execute sudo command, password prompts cannot be handled");
        return nullptr;
    }

    // Log the command being executed
    lln("\\ouc{#00ff00ff}Executing shell command: \\ouc{#ffff00ff}%s", shell_cmd);
    console_draw_separator();

    // Execute command
    C8 full_cmd[MAX_SHELL_CMD_LENGTH + 128] = {};
    ou_snprintf(full_cmd, sizeof(full_cmd), "%s 2>&1", shell_cmd);
    fp = popen(full_cmd, "r");
    if (!fp) {
        llw("Could not execute shell command: %s", shell_cmd);
        return nullptr;
    }

    // Read output character by character
    C8 buffer[1024] = {};
    SZ buffer_pos   = 0;

    S32 c = 0;
    while ((c = fgetc(fp)) != EOF) {
        if (c == '\n' || buffer_pos >= sizeof(buffer) - 1) {
            buffer[buffer_pos] = '\0';

            // Strip ANSI color codes from the output
            i_strip_ansi_color_codes(buffer);

            // Look for error/warning/error indicators at line level (both direct and embedded in timestamp blocks)
            BOOL is_error_line   = false;
            BOOL is_warning_line = false;

            // Check for error or warning keywords
            if (ou_strstr(buffer, "] ERR ") || ou_strstr(buffer, "] WRN ") || ou_strstr(buffer, "error") || ou_strstr(buffer, "Error") ||
                ou_strstr(buffer, "ERROR") || ou_strstr(buffer, "failed") || ou_strstr(buffer, "Failed") || ou_strstr(buffer, "FAILED")) {
                is_error_line = true;
            } else if (ou_strstr(buffer, "warning") || ou_strstr(buffer, "Warning") || ou_strstr(buffer, "WARNING")) {
                is_warning_line = true;
            }

            // Apply appropriate color to entire line
            if (is_error_line) {
                lln("\\ouc{#ff0000ff}%s", buffer);  // Red for errors
            } else if (is_warning_line) {
                lln("\\ouc{#ffff00ff}%s", buffer);  // Yellow for warnings
            } else {
                lln("\\ouc{#aaaaafff}%s", buffer);  // Light gray for normal output
            }

            buffer_pos = 0;

            if (c == '\n') { continue; }
        }

        buffer[buffer_pos++] = (C8)c;
    }

    // Output any remaining partial line
    if (buffer_pos > 0) {
        buffer[buffer_pos] = '\0';
        i_strip_ansi_color_codes(buffer);  // Strip ANSI color codes from the final output
        lln("\\ouc{#aaaaafff}%s", buffer);
    }

    S32 const status = pclose(fp);
    if (status != 0) {
        llw("Command exited with status: %d", status);
    } else {
        lln("\\ouc{#00ff00ff}Command completed successfully.");
    }

    console_draw_separator();
    return nullptr;
}

BOOL con_cmd_shell(ConCMD const *cmd) {
    C8 shell_cmd[MAX_SHELL_CMD_LENGTH] = {};
    for (SZ i = 0; cmd->args[i] != nullptr; ++i) {
        ou_strncat(shell_cmd, cmd->args[i], sizeof(shell_cmd) - ou_strlen(shell_cmd) - 1);
        ou_strncat(shell_cmd, " ", sizeof(shell_cmd) - ou_strlen(shell_cmd) - 1);
    }

    if (shell_cmd[0] == '\0') {
        llw("Shell command is empty");
        return false;
    }

    pthread_t thread_id = 0;
    C8 *shell_cmd_copy  = ou_strdup(shell_cmd, MEMORY_TYPE_DARENA);
    pthread_create(&thread_id, nullptr, i_execute_shell_cmd, (void *)shell_cmd_copy);
    pthread_detach(thread_id);

    return true;
}

#else

BOOL con_cmd_shell(const ConsoleCMD *cmd) {
    llw("Command shell is not supported on Windows");
    return false;
}

#endif

BOOL con_cmd_echo(ConCMD const *cmd) {
    C8 *level_str = cmd->args[0];
    if (!level_str) {
        llw("Could not parse level");
        return false;
    }

    C8 *message = cmd->args[1];
    if (!message) {
        llw("Could not parse message");
        return false;
    }

    LLogLevel level = LLOG_LEVEL_NONE;
    switch (level_str[0]) {
        case 't': {
            level = LLOG_LEVEL_TRACE;
        } break;
        case 'd': {
            level = LLOG_LEVEL_DEBUG;
        } break;
        case 'i': {
            level = LLOG_LEVEL_INFO;
        } break;
        case 'w': {
            level = LLOG_LEVEL_WARN;
        } break;
        case 'e': {
            level = LLOG_LEVEL_ERROR;
        } break;
        case 'f': {
            level = LLOG_LEVEL_FATAL;
        } break;
        case 'm': {
            switch (level_str[1]) {  // It's a message ergo we need to check the second character.
                case 'i': {
                    lln("Message sent: %s", message);
                    mi(message, WHITE);
                    return true;
                };
                case 'w': {
                    lln("Message sent: %s", message);
                    mw(message, ORANGE);
                    return true;
                };
                case 'e': {
                    lln("Message sent: %s", message);
                    me(message, RED);
                    return true;
                };
                default: {
                    llw("Unknown ll level: %s", level_str);
                    return false;
                };
            }
        } break;
        default: {
            llw("Unknown ll level: %s", level_str);
            return false;
        }
    }

    // Check if the ll level is enabled.
    if (!(llog_get_level() <= level)) { llw("Log level %s is not enabled", llog_level_to_cstr(level)); }

    lll(level, "%s", message);

    return true;
}

BOOL con_cmd_clear(ConCMD const *cmd) {
    unused(cmd);

    console_clear();

    return true;
}

BOOL con_cmd_exit(ConCMD const *cmd) {
    if (cmd->args[0] != nullptr) {
        llw("Command exit does not take any arguments");
        return false;
    }

    g_core.running = false;

    return true;
}

BOOL con_cmd_cam_to_cb(ConCMD const *cmd) {
    unused(cmd);

    Camera3D *cam_ptr = c3d_get_ptr();
    String *camera_code_string = TS("POS: {%.2fF, %.2fF, %.2fF} TAR: {%.2fF, %.2fF, %.2fF}",
                                    cam_ptr->position.x, cam_ptr->position.y, cam_ptr->position.z,
                                    cam_ptr->target.x, cam_ptr->target.y, cam_ptr->target.z);
    SetClipboardText(camera_code_string->c);
    lln("Copied camera info to clipboard: %s", camera_code_string->c);

    return true;
}

BOOL con_cmd_history(ConCMD const *cmd) {
    if (cmd->args[0] != nullptr) {
        llw("Command history does not take any arguments");
        return false;
    }

    if (g_console.cmd_history_buffer.count == 0) {
        lln("No command history available");
        return true;
    }

    console_draw_separator();
    lln("Command History (%zu entries):", g_console.cmd_history_buffer.count);

    for (SZ i = 0; i < g_console.cmd_history_buffer.count; ++i) {
        C8 const *hcmd = ring_get(&g_console.cmd_history_buffer, i);
        if (hcmd) { lln("%zu: %s", i + 1, hcmd); }
    }

    return true;
}

SZ static i_get_visible_line_count() {
    Vector2 const res   = render_get_render_resolution();
    F32 const font_size = (F32)c_console__font_size;
    SZ const lines      = (SZ)((res.y - (font_size * 2.0F)) / font_size);
    if (c_console__fullscreen) { return lines; }
    return lines / 2;
}

void static i_save_history_to_file() {
    if (g_console.cmd_history_buffer.count == 0) { return; }

    String *history_text = string_create_empty(MEMORY_TYPE_TARENA);

    for (SZ i = 0; i < g_console.cmd_history_buffer.count; ++i) {
        C8 const *cmd = ring_get(&g_console.cmd_history_buffer, i);
        if (cmd) {
            string_append(history_text, cmd);
            string_append(history_text, "\n");
        }
    }

    if (!SaveFileText(CONSOLE_HISTORY_FILE_NAME, history_text->c)) { lle("could not save console history to %s", CONSOLE_HISTORY_FILE_NAME); }
}

void static i_load_history_from_file() {
    if (!FileExists(CONSOLE_HISTORY_FILE_NAME)) { return; }

    C8 *file_contents = LoadFileText(CONSOLE_HISTORY_FILE_NAME);
    if (!file_contents) { return; }

    C8 *saveptr = nullptr;
    C8 const *line = ou_strtok(file_contents, "\n", &saveptr);

    while (line) {
        if (ou_strlen(line) > 0) { ring_push(&g_console.cmd_history_buffer, ou_strdup(line, MEMORY_TYPE_DARENA)); }
        line = ou_strtok(nullptr, "\n", &saveptr);
    }

    UnloadFileText(file_contents);
}

void console_init() {
    g_console.scrollbar = asset_get_texture("cursor_re_vertical.png");

    ring_init(MEMORY_TYPE_DARENA, &g_console.output_buffer, 1000);
    ring_init(MEMORY_TYPE_DARENA, &g_console.cmd_history_buffer, 1000);

    g_console.visible_line_count = i_get_visible_line_count();
    g_console.initialized        = true;

    i_load_history_from_file();
}

void console_update() {
    if (!c_console__enabled) { return; }

    Vector2 static last_res = {};
    Vector2 const res       = render_get_render_resolution();;

    if (!Vector2Equals(last_res, res)) {
        g_console.visible_line_count = i_get_visible_line_count();
        last_res = res;
    }

    if (is_pressed(IA_DBG_TOGGLE_CONSOLE_FULLSCREEN)) {
        c_console__fullscreen        = !c_console__fullscreen;
        g_console.visible_line_count = i_get_visible_line_count();
    }

    if (is_mod(I_MODIFIER_SHIFT)) {
        BOOL has_moved = false;

        if (is_pressed_or_repeat(IA_CONSOLE_SCROLL_UP)) {
            SZ font_size = (SZ)c_console__font_size;
            if (font_size == 0) { font_size = CONSOLE_FONT_SIZE; }
            SZ const max_lines = (((SZ)res.y - font_size) / font_size);

            g_console.visible_line_count++;
            g_console.visible_line_count = glm::min(g_console.visible_line_count, max_lines);

            has_moved = true;
        }

        if (is_pressed_or_repeat(IA_CONSOLE_SCROLL_DOWN)) {
            g_console.visible_line_count--;
            g_console.visible_line_count = glm::max(g_console.visible_line_count, (SZ)CONSOLE_LINE_COUNT_MIN);

            has_moved = true;
        }

        if (has_moved) {
            c_console__fullscreen = false;
            g_console.vertical_history_offset = 0;
        }
    }

    if (is_pressed_or_repeat(IA_CONSOLE_INCREASE_FONT_SIZE)) { c_console__font_size += 1; }
    if (is_pressed_or_repeat(IA_CONSOLE_DECREASE_FONT_SIZE)) { c_console__font_size -= 1; }

    // Always check if we're exceeding screen bounds after any font size change
    SZ const max_lines = (SZ)((res.y - ((F32)c_console__font_size * 2.0F)) / (F32)(c_console__font_size));
    // Cap the visible line count if it exceeds maximum
    g_console.visible_line_count = glm::min(g_console.visible_line_count, max_lines);

    if (is_pressed_or_repeat(IA_CONSOLE_BEGIN_LINE)) { console_do_move_cursor_begin_line(); }
    if (is_pressed_or_repeat(IA_CONSOLE_END_LINE))   { console_do_move_cursor_end_line(); }
    if (is_pressed_or_repeat(IA_CONSOLE_KILL_LINE))  { console_do_kill_line(); }
    if (is_pressed_or_repeat(IA_CONSOLE_COPY))       { console_do_copy(); }
    if (is_pressed_or_repeat(IA_CONSOLE_PASTE))      { console_do_paste(); }

    F32 const mouse_wheel = input_get_mouse_wheel();

    // INFO: !is_mod(I_MODIFIER_SHIFT) we are checking that because when we look in debug.cpp, we also do something with the console and shift.
    // We don't want to scroll the console when we are holding shift and doing the other thing.

    if (!is_mod(I_MODIFIER_SHIFT) && g_console.vertical_history_offset < g_console.output_buffer.count) {
        if (mouse_wheel > 0.0F || is_pressed_or_repeat(IA_CONSOLE_SCROLL_UP)) { g_console.vertical_history_offset++; }
    }

    if (g_console.vertical_history_offset > 0) {
        if (!is_mod(I_MODIFIER_SHIFT) && (mouse_wheel < 0.0F || is_pressed_or_repeat(IA_CONSOLE_SCROLL_DOWN))) { g_console.vertical_history_offset--; }
    }

    S32 key = GetCharPressed();
    while (key > 0) {
        if ((key >= 32) && (key <= 125) && (g_console.input_buffer_cursor < CON_IN_BUF_MAX - 1)) {
            // Shift the string to the right, starting from the end, to make space.
            SZ const buffer_length = ou_strlen(g_console.input_buffer);
            if (buffer_length < CON_IN_BUF_MAX - 1) {
                for (SZ i = buffer_length; i > g_console.input_buffer_cursor; --i) { g_console.input_buffer[i] = g_console.input_buffer[i - 1]; }
                g_console.input_buffer[g_console.input_buffer_cursor++] = (C8)key;
                g_console.input_buffer[buffer_length + 1] = '\0';
            }
        }

        key = GetCharPressed();
    }

    if (is_pressed_or_repeat(IA_CONSOLE_BACKSPACE))        { console_do_backspace(); }
    if (is_pressed_or_repeat(IA_CONSOLE_DELETE))           { console_do_delete(); }
    if (is_pressed_or_repeat(IA_CONSOLE_HISTORY_PREVIOUS)) { console_do_history_previous(); }
    if (is_pressed_or_repeat(IA_CONSOLE_HISTORY_NEXT))     { console_do_history_next(); }

    BOOL const ctrl_down = is_mod(I_MODIFIER_CTRL);

    if (is_pressed_or_repeat(IA_CONSOLE_CURSOR_LEFT)) {
        if (!ctrl_down) {
            console_do_move_cursor(-1);
        } else {
            console_do_move_cursor_word(-1);
        }
    }

    if (is_pressed_or_repeat(IA_CONSOLE_CURSOR_RIGHT)) {
        if (!ctrl_down) {
            console_do_move_cursor(1);
        } else {
            console_do_move_cursor_word(1);
        }
    }

    console_fill_prediction_buffer();

    if (is_pressed(IA_CONSOLE_AUTOCOMPLETE)) {
        if (console_autocomplete_input_buffer()) { g_console.input_buffer_cursor = ou_strlen(g_console.input_buffer); }
    }

    if (is_pressed(IA_CONSOLE_EXECUTE)) {
        // Reset history offset if the user presses enter.
        g_console.vertical_history_offset = 0;
        g_console.cmd_history_cursor = 0;
        console_do_parse_input();
    }
}

void static i_populate_console_output(C8 const *cstr, SZ idx, void *console_info) {
    unused(idx);

    if (cstr == nullptr) { return; }

    auto *info          = (IConsoleRenderInfo *)console_info;
    info->position.y   -= (F32)info->font->font_size;
    C8 const *split_pos = ou_strchr(cstr, '|');

    if (split_pos != nullptr) {
        C8 first_part[512]  = {};
        C8 second_part[512] = {};

        // We remove the "|" character from the first part.
        SZ const first_part_length = (SZ)(split_pos - cstr);

        ou_strncpy(first_part, cstr, first_part_length);
        first_part[first_part_length] = '\0';
        ou_strncpy(second_part, split_pos + 1, ou_strlen(split_pos + 1));

        IConsoleColorPair prefix_color_pair = i_output_colors_for_prefix[0];
        switch (first_part[0]) {
            case ' ': {
                d2d_text_ouc(info->font, second_part, info->position, CONSOLE_OUTPUT_FOREGROUND_COLOR);
                return;
            }
            case 'T': {
                prefix_color_pair = i_output_colors_for_prefix[1];
            } break;
            case 'D': {
                prefix_color_pair = i_output_colors_for_prefix[2];
            } break;
            case 'I': {
                prefix_color_pair = i_output_colors_for_prefix[3];
            } break;
            case 'W': {
                prefix_color_pair = i_output_colors_for_prefix[4];
            } break;
            case 'E': {
                prefix_color_pair = i_output_colors_for_prefix[5];
            } break;
            case 'F': {
                prefix_color_pair = i_output_colors_for_prefix[6];
            } break;
            default:
                break;
        }

        d2d_text_ouc(info->font, first_part, info->position, color_sync_blinking_regular(prefix_color_pair.color_a, prefix_color_pair.color_b));
        Vector2 const first_part_size = measure_text_ouc(info->font, first_part);
        Vector2 const other_position  = {info->position.x + first_part_size.x, info->position.y};
        d2d_text_ouc(info->font, second_part, other_position, CONSOLE_OUTPUT_FOREGROUND_COLOR);
    } else {
        // We do a similar thing with ":" as we do with "|".
        // We split the string into two parts and color the first part differently.
        // This is useful since we use this to output key-value pairs usually.
        // But we first make sure that the string contains a ":" and has no spaces in the first part.
        split_pos = ou_strchr(cstr, ':');
        BOOL has_spaces = false;

        if (split_pos != nullptr) {
            for (C8 const *it = cstr; it != split_pos; ++it) {
                if (*it == ' ') {
                    has_spaces = true;
                    break;
                }
            }
        }

        if (split_pos != nullptr && !has_spaces) {
            C8 first_part[512]         = {};
            C8 second_part[512]        = {};
            SZ const first_part_length = (SZ)(split_pos - cstr + 1);

            ou_strncpy(first_part, cstr, first_part_length);
            first_part[first_part_length] = '\0';
            ou_strncpy(second_part, split_pos + 1, ou_strlen(split_pos + 1));

            d2d_text_ouc(info->font, first_part, info->position, color_sync_blinking_regular(GREEN, LIME));
            Vector2 const first_part_size = measure_text_ouc(info->font, first_part);
            Vector2 const other_position  = {info->position.x + first_part_size.x, info->position.y};
            d2d_text_ouc(info->font, second_part, other_position, CONSOLE_OUTPUT_FOREGROUND_COLOR);
            return;
        }

        // Now check this, you will not believe what happens next.
        // When we receive "----" here, we want to draw a line.
        // Yes, a yellow line, full width of the console.
        // Insane, right? This is gold. I'm telling you. GOLD.
        if (ou_strcmp(cstr, "----") == 0) {
            F32 const line_width = (F32)info->font->font_size * 0.10F;
            Rectangle const line_bounds = {
                info->position.x,
                info->position.y + ((F32)info->font->font_size / 2.0F) - (line_width / 2.0F),
                info->size.x,
                line_width,
            };
            d2d_rectangle_rec(line_bounds, Fade(color_sync_blinking_regular(PURPLE, MAGENTA), 0.25F));
            return;
        }

        // This is gonna sound crazy, but we need to check for escape sequences.
        // If we find one, we need to remove it and draw the text without it.
        if (ou_strchr(cstr, '\033') != nullptr) {
            String *formatted_string = TS("%s", cstr);
            string_remove_shell_escape_sequences(formatted_string);
            d2d_text_ouc(info->font, formatted_string->c, info->position, color_sync_blinking_regular(GOLD, YELLOW));

            return;
        }

        // Okay, let's color also the line if it starts with something like [1/10] or [15/15].
        if (cstr[0] == '[') {
            C8 const *end_pos = ou_strchr(cstr, ']');
            if (end_pos != nullptr) {
                C8 const *slash_pos = ou_strchr(cstr, '/');
                if (slash_pos != nullptr && slash_pos < end_pos) {
                    d2d_text_ouc(info->font, cstr, info->position, color_sync_blinking_regular(ORANGE, SALMON));
                    return;
                }
            }
        }

        // If we reach this point, we just draw the text as is. No fancy stuff.
        d2d_text_ouc(info->font, cstr, info->position, CONSOLE_OUTPUT_FOREGROUND_COLOR);
    }
}

void console_draw() {
    Vector2 const res = render_get_render_resolution();;
    AFont *font       = asset_get_font(c_console__font.cstr, c_console__font_size);
    if (font == nullptr) {
        // Fallback to default and report
        llw("Font (%s) could not be found, falling back to default font", c_console__font.cstr);
        c_console__font = CVarStr(CONSOLE_FONT);
        font = asset_get_font(c_console__font.cstr, c_console__font_size);
    }

    F32 const inner_padding           = (F32)(S32)((F32)font->font_size * 0.33F);
    F32 const input_background_height = (F32)(S32)((F32)font->font_size + (inner_padding * 2.0F));

    // Output background.
    F32 const total_height = ((F32)font->font_size * (F32)g_console.visible_line_count) + (inner_padding * 2.0F);
    Rectangle const output_background_bounds = {
        0.0F,  res.y - total_height - input_background_height,
        res.x, total_height + input_background_height,
    };
    d2d_rectangle_rec(output_background_bounds, CONSOLE_OUTPUT_BACKGROUND_COLOR);

    // Output buffer.
    IConsoleRenderInfo info = {
        {inner_padding, inner_padding + output_background_bounds.y + ((F32)font->font_size * (F32)g_console.visible_line_count)},
        {res.x - (inner_padding * 4.0F), output_background_bounds.height - (inner_padding * 2.0F)},
        font,
    };
    // Iterate through output buffer from newest to oldest entries with offset
    SZ const output_count = g_console.output_buffer.count;
    if (output_count > 0) {
        SZ const start_offset = glm::min(g_console.vertical_history_offset, output_count - 1);
        SZ const max_entries  = glm::min(g_console.visible_line_count, output_count);

        for (SZ i = 0; i < max_entries && (start_offset + i) < output_count; i++) {
            SZ const idx   = output_count - 1 - start_offset - i;
            C8 const *cstr = ring_get(&g_console.output_buffer, idx);
            i_populate_console_output(cstr, i, &info);
        }
    }

    // Input background.
    Rectangle const input_background_bounds = {
        0.0F,
        res.y - input_background_height,
        res.x - ((F32)g_console.scrollbar->base.width * 1.5F),
        input_background_height,
    };
    d2d_rectangle_rec(input_background_bounds, CONSOLE_INPUT_BACKGROUND_COLOR);

    // Input buffer.
    Vector2 const input_buffer_position = {
        inner_padding,
        input_background_bounds.y + inner_padding,
    };
    // Prediction buffer first.
    d2d_text_ouc(font, g_console.prediction_buffer, input_buffer_position, CONSOLE_PREDICTION_FOREGROUND_COLOR);
    d2d_text_ouc(font, g_console.input_buffer, input_buffer_position, CONSOLE_INPUT_FOREGROUND_COLOR);

    // Input cursor.
    // Ensure cursor idx is within the string length.
    SZ cursor_idx         = g_console.input_buffer_cursor;
    SZ const input_length = ou_strlen(g_console.input_buffer);
    cursor_idx            = glm::min(cursor_idx, input_length);

    // Get the substring of input_buffer up to the cursor.
    C8 cursor_substring[CON_IN_BUF_MAX];
    ou_strncpy(cursor_substring, g_console.input_buffer, cursor_idx);
    cursor_substring[cursor_idx] = '\0';

    // Calculate the cursor position.
    Vector2 const cursor_position = {input_buffer_position.x + measure_text_ouc(font, cursor_substring).x, input_buffer_position.y};

    // Draw the cursor.
    d2d_rectangle_rec({cursor_position.x, cursor_position.y, (F32)font->font_size * 0.10F, (F32)font->font_size}, CONSOLE_INPUT_CURSOR_COLOR);

    // Scrollbar.
    SZ const vertical_history_offset = g_console.vertical_history_offset;

    F32 const scrollbar_y = output_background_bounds.y + ((F32)g_console.visible_line_count * (F32)font->font_size * (1.0F - (F32)vertical_history_offset / (F32)g_console.output_buffer.count));

    // Draw the scrollbar.
    Rectangle const scrollbar_rec = {
        res.x - ((F32)g_console.scrollbar->base.width * 1.5F), output_background_bounds.y,
        (F32)g_console.scrollbar->base.width * 1.5F,           output_background_bounds.height,
    };
    d2d_rectangle_rec(scrollbar_rec, CONSOLE_SCROLLBAR_COLOR);

    d2d_texture(g_console.scrollbar, res.x - ((F32)g_console.scrollbar->base.width * 1.25F), scrollbar_y, CONSOLE_OUTPUT_FOREGROUND_COLOR);
}

void static i_reset_console_input() {
    g_console.input_buffer_cursor = 0;
    g_console.input_buffer[0]     = '\0';
    g_console.cmd_history_cursor  = 0;
}

void console_do_parse_input() {
    console_draw_separator();

    // Do nothing if it's an empty string or only whitespace but reset the cursor.
    if (ou_strlen(g_console.input_buffer) == 0 || ou_strspn(g_console.input_buffer, " ") == ou_strlen(g_console.input_buffer)) {
        i_reset_console_input();
        return;
    }

    BOOL is_duplicate = false;
    if (g_console.cmd_history_buffer.count > 0) {
        C8 const *last_cmd = ring_peek_last(&g_console.cmd_history_buffer);
        if (last_cmd && ou_strcmp(last_cmd, g_console.input_buffer) == 0) { is_duplicate = true; }
    }

    if (!is_duplicate) {
        ring_push(&g_console.cmd_history_buffer, ou_strdup(g_console.input_buffer, MEMORY_TYPE_DARENA));
        i_save_history_to_file();
    }

    // First check for CVar operations before looking for commands

    // Check for assignment operation (contains '=')
    C8 const *equals_pos = ou_strchr(g_console.input_buffer, '=');
    if (equals_pos != nullptr) {
        // Process as CVar assignment
        // Extract cvar name (left of '=')
        C8 cvar_name[CVAR_NAME_MAX_LENGTH] = {};
        SZ const name_len = (SZ)(equals_pos - g_console.input_buffer);
        ou_strncpy(cvar_name, g_console.input_buffer, name_len);
        cvar_name[name_len] = '\0';

        // Trim whitespace from name
        SZ trim_start = 0;
        SZ trim_end   = name_len - 1;
        while (trim_start < name_len && cvar_name[trim_start] == ' ') { trim_start++; }
        while (trim_end > trim_start && cvar_name[trim_end] == ' ') { trim_end--; }

        C8 trimmed_name[CVAR_NAME_MAX_LENGTH] = {};
        ou_strncpy(trimmed_name, cvar_name + trim_start, trim_end - trim_start + 1);
        trimmed_name[trim_end - trim_start + 1] = '\0';

        // Extract value (right of '=')
        C8 cvar_value[CON_IN_BUF_MAX] = {};
        ou_strncpy(cvar_value, equals_pos + 1, CON_IN_BUF_MAX);

        // Trim whitespace from value
        SZ const val_len = ou_strlen(cvar_value);
        trim_start       = 0;
        trim_end         = val_len - 1;
        while (trim_start < val_len && cvar_value[trim_start] == ' ') { trim_start++; }
        while (trim_end > trim_start && cvar_value[trim_end] == ' ') { trim_end--; }

        C8 trimmed_value[CON_IN_BUF_MAX] = {};
        ou_strncpy(trimmed_value, cvar_value + trim_start, trim_end - trim_start + 1);
        trimmed_value[trim_end - trim_start + 1] = '\0';

        // Check if the CVar exists and set its value
        for (const auto &cvar : cvar_meta_table) {
            if (ou_strcmp(cvar.name, trimmed_name) == 0) {
                switch (cvar.type) {
                    case CVAR_TYPE_BOOL: {
                        BOOL val = false;
                        if (ou_strcmp(trimmed_value, "true") == 0 || ou_strcmp(trimmed_value, "1") == 0) {
                            val = true;
                        } else if (ou_strcmp(trimmed_value, "false") == 0 || ou_strcmp(trimmed_value, "0") == 0) {
                            val = false;
                        } else {
                            llw("Invalid BOOL value for CVar %s: %s (use true/false or 1/0)", trimmed_name, trimmed_value);
                            i_reset_console_input();
                            return;
                        }
                        *(BOOL *)cvar.address = val;
                        lln("Set CVar \\ouc{#ffff00ff}%s \\ouc{#00ff00ff}= %s", trimmed_name, BOOL_TO_STR(val));
                        i_reset_console_input();
                        return;
                    }
                    case CVAR_TYPE_S32: {
                        S32 val = 0;
                        if (ou_sscanf(trimmed_value, "%d", &val) != 1) {
                            llw("Could not parse value as S32 for CVar %s: %s", trimmed_name, trimmed_value);
                            i_reset_console_input();
                            return;
                        }
                        *(S32 *)cvar.address = val;
                        lln("Set CVar \\ouc{#ffff00ff}%s \\ouc{#00ff00ff}= %d", trimmed_name, val);
                        i_reset_console_input();
                        return;
                    }
                    case CVAR_TYPE_F32: {
                        F32 val = 0.0F;
                        if (ou_sscanf(trimmed_value, "%f", &val) != 1) {
                            llw("Could not parse value as F32 for CVar %s: %s", trimmed_name, trimmed_value);
                            i_reset_console_input();
                            return;
                        }
                        *(F32 *)cvar.address = val;
                        lln("Set CVar \\ouc{#ffff00ff}%s \\ouc{#00ff00ff}= %f", trimmed_name, val);
                        i_reset_console_input();
                        return;
                    }
                    case CVAR_TYPE_CVARSTR: {
                        CVarStr val = {0};
                        ou_strncpy(val.cstr, trimmed_value, CVAR_STR_MAX_LENGTH - 1);
                        val.cstr[CVAR_STR_MAX_LENGTH - 1] = '\0';  // Ensure null termination
                        *(CVarStr *)cvar.address = val;
                        lln("Set CVar \\ouc{#ffff00ff}%s \\ouc{#00ff00ff}= %s", trimmed_name, val.cstr);
                        i_reset_console_input();
                        return;
                    }
                    default: {
                        _unreachable_();
                    }
                }
            }
        }

        llw("CVar with name %s not found", trimmed_name);
        i_reset_console_input();
        return;
    }

    // Check for toggle operation (ends with ' t')
    SZ const input_len = ou_strlen(g_console.input_buffer);
    if (input_len >= 3 && g_console.input_buffer[input_len - 2] == ' ' && g_console.input_buffer[input_len - 1] == 't') {
        // Extract cvar name
        C8 cvar_name[CVAR_NAME_MAX_LENGTH] = {};
        ou_strncpy(cvar_name, g_console.input_buffer, input_len - 2);
        cvar_name[input_len - 2] = '\0';

        // Trim trailing whitespace
        SZ trim_end = input_len - 3;
        while (trim_end > 0 && cvar_name[trim_end] == ' ') { trim_end--; }
        cvar_name[trim_end + 1] = '\0';

        // Find the CVar to determine if it's a boolean
        for (const auto &cvar : cvar_meta_table) {
            if (ou_strcmp(cvar.name, cvar_name) == 0) {
                if (cvar.type == CVAR_TYPE_BOOL) {
                    *(BOOL *)cvar.address = !*(BOOL *)cvar.address;
                    lln("Toggled CVar \\ouc{#ffff00ff}%s \\ouc{#00ff00ff}= %s", cvar_name, BOOL_TO_STR(*(BOOL *)cvar.address));
                    i_reset_console_input();
                    return;
                }
                llw("Cannot toggle non-boolean CVar %s (type: %d)", cvar_name, cvar.type);
                i_reset_console_input();
                return;
            }
        }

        llw("CVar with name %s not found", cvar_name);
        i_reset_console_input();
        return;
    }

    // Now check for regular commands
    C8 cmd_parse_buf[CON_IN_BUF_MAX];
    ou_strncpy(cmd_parse_buf, g_console.input_buffer, CON_IN_BUF_MAX - 1);
    cmd_parse_buf[CON_IN_BUF_MAX - 1] = '\0';

    C8 *cmd_saveptr = nullptr;
    C8 *cmd_token   = ou_strtok(cmd_parse_buf, " ", &cmd_saveptr);
    if (!cmd_token) {
        llw("Could not parse console command");
        i_reset_console_input();
        return;
    }

    ConCMD cmd     = {};
    C8 *cmd_string = cmd_token;

    // First token is the command name.
    for (auto const &i_cmd : i_cmds) {
        if (ou_strcmp(cmd_token, i_cmd.name) != 0) { continue; }

        cmd = i_cmd;

        // The rest of the tokens are the command arguments.
        SZ arg_count = 0;
        while ((cmd_token = ou_strtok(nullptr, " ", &cmd_saveptr))) { cmd.args[arg_count++] = cmd_token; }

        if (!cmd.func(&cmd)) { llw("Could not execute console command: '%s' (Example: '%s')", cmd.name, cmd.example); }

        i_reset_console_input();
        return;
    }

    // Check if it's a get operation (just a CVar name)
    cmd_token = cmd_string;  // Reset token to the first part

    for (const auto &cvar : cvar_meta_table) {
        if (ou_strcmp(cvar.name, cmd_token) == 0) {
            // Prepare comment string
            C8 comment_part[256] = "";
            if (cvar.comment[0] != '\0') {
                ou_snprintf(comment_part, sizeof(comment_part), " \\ouc{#888888ff}# %s", cvar.comment);
            }

            switch (cvar.type) {
                case CVAR_TYPE_BOOL: {
                    lln("CVar \\ouc{#ffff00ff}%s \\ouc{#00ff00ff}= %s (type: BOOL)%s", cvar.name, BOOL_TO_STR(*((BOOL *)cvar.address)), comment_part);
                    i_reset_console_input();
                    return;
                }
                case CVAR_TYPE_S32: {
                    lln("CVar \\ouc{#ffff00ff}%s \\ouc{#00ff00ff}= %d (type: S32)%s", cvar.name, *((S32 *)cvar.address), comment_part);
                    i_reset_console_input();
                    return;
                }
                case CVAR_TYPE_F32: {
                    lln("CVar \\ouc{#ffff00ff}%s \\ouc{#00ff00ff}= %.8f (type: F32)%s", cvar.name, *((F32 *)cvar.address), comment_part);
                    i_reset_console_input();
                    return;
                }
                case CVAR_TYPE_CVARSTR: {
                    lln("CVar \\ouc{#ffff00ff}%s \\ouc{#00ff00ff}= %s (type: CVarStr)%s", cvar.name, (*((CVarStr *)cvar.address)).cstr, comment_part);
                    i_reset_console_input();
                    return;
                }
                default: {
                    _unreachable_();
                }
            }
        }
    }

    // Try to find approximate command. This is useful for typos.
    // We just iterate over the commands and find the one with the smallest levenshtein distance.
    SZ min_distance     = SZ_MAX;
    SZ min_distance_idx = 0;

    for (SZ i = 0; i < CON_CMD_TYPE_COUNT; ++i) {
        SZ const distance = math_levenshtein_distance(cmd_string, i_cmds[i].name);
        if (distance < min_distance) {
            min_distance     = distance;
            min_distance_idx = i;
        }
    }

    if (min_distance < 4) {
        llw("Could not find console command or CVar '%s'. Did you mean '%s'?", cmd_string, i_cmds[min_distance_idx].name);
        ou_strncpy(g_console.input_buffer, i_cmds[min_distance_idx].name, CON_IN_BUF_MAX);
        g_console.input_buffer_cursor = ou_strlen(g_console.input_buffer);
        return;
    }

    for (SZ i = 0; i < CVAR_COUNT; ++i) {
        CVarMeta const *cvar = &cvar_meta_table[i];
        SZ const distance = math_levenshtein_distance(cmd_string, cvar->name);
        if (distance < min_distance) {
            min_distance     = distance;
            min_distance_idx = i;
        }
    }

    if (min_distance < 4) {
        llw("Could not find console command or CVar '%s'. Did you mean '%s'?", cmd_string, cvar_meta_table[min_distance_idx].name);
        ou_strncpy(g_console.input_buffer, cvar_meta_table[min_distance_idx].name, CON_IN_BUF_MAX);
        g_console.input_buffer_cursor = ou_strlen(g_console.input_buffer);
        return;
    }

    lln("\\ouc{#ffaa00ff}No matching console command or CVar found, attempting as shell command...");

    // Find the shell command
    ConCMD shell_cmd = {};
    BOOL found_shell_cmd = false;

    for (auto const &i_cmd : i_cmds) {
        if (ou_strcmp(i_cmd.name, "s") == 0) {
            shell_cmd       = i_cmd;
            found_shell_cmd = true;
            break;
        }
    }

    if (!found_shell_cmd) {
        llw("Could not find shell command 's' in command list");
        i_reset_console_input();
        return;
    }

    // Set the first argument to the entire input string
    C8 *shell_arg = ou_strdup(g_console.input_buffer, MEMORY_TYPE_TARENA);
    shell_cmd.args[0] = shell_arg;

    // Execute as shell command
    BOOL const result = shell_cmd.func(&shell_cmd);
    if (!result) { llw("Failed to execute as shell command: '%s'", g_console.input_buffer); }

    i_reset_console_input();
}

void console_do_move_cursor(S32 offset) {
    // Calculate new cursor position with the given offset.
    S32 new_position = (S32)g_console.input_buffer_cursor + offset;

    // Ensure the new position is within bounds.
    if (new_position < 0) {
        new_position = 0;
    } else if (new_position > (S32)ou_strlen(g_console.input_buffer)) {
        new_position = (S32)ou_strlen(g_console.input_buffer);
    }

    // Update cursor position.
    g_console.input_buffer_cursor = (SZ)new_position;
}

void console_do_move_cursor_word(S32 direction) {
    if (direction == -1) {  // Move backward.
        // Skip trailing spaces to land on the boundary of a word.
        while (g_console.input_buffer_cursor > 0 && g_console.input_buffer[g_console.input_buffer_cursor - 1] == ' ') { console_do_move_cursor(-1); }
        // Skip the word itself to land at its beginning.
        while (g_console.input_buffer_cursor > 0 && g_console.input_buffer[g_console.input_buffer_cursor - 1] != ' ') { console_do_move_cursor(-1); }
    } else if (direction == 1) {  // Move forward.
        SZ const buffer_length = ou_strlen(g_console.input_buffer);
        // Skip leading spaces to land on the boundary of the next word.
        while (g_console.input_buffer_cursor < buffer_length && g_console.input_buffer[g_console.input_buffer_cursor] == ' ') { console_do_move_cursor(1); }
        // Skip the word itself to land just after it.
        while (g_console.input_buffer_cursor < buffer_length && g_console.input_buffer[g_console.input_buffer_cursor] != ' ') { console_do_move_cursor(1); }
    }
}

void console_do_backspace() {
    if (g_console.input_buffer_cursor > 0) {
        // Shift the rest of the string to the left to remove the character.
        for (SZ i = g_console.input_buffer_cursor - 1; g_console.input_buffer[i] != '\0'; ++i) { g_console.input_buffer[i] = g_console.input_buffer[i + 1]; }
        g_console.input_buffer_cursor--;
    }
}

void console_do_delete() {
    SZ const buffer_length = ou_strlen(g_console.input_buffer);
    if (g_console.input_buffer_cursor < buffer_length) {
        // Shift the string left from the cursor position to delete the character.
        for (SZ i = g_console.input_buffer_cursor; i < buffer_length; ++i) { g_console.input_buffer[i] = g_console.input_buffer[i + 1]; }
    }
}

void console_do_history_previous() {
    BOOL const any_cmds_in_history = g_console.cmd_history_buffer.count > 0;
    BOOL const cursor_not_at_end   = g_console.cmd_history_cursor < g_console.cmd_history_buffer.count;

    if (any_cmds_in_history && cursor_not_at_end) {
        SZ const latest_idx = g_console.cmd_history_buffer.count - 1;
        SZ const peek_idx   = latest_idx - g_console.cmd_history_cursor;

        C8 const *cmd = ring_get(&g_console.cmd_history_buffer, peek_idx);
        if (cmd) {
            ou_strncpy(g_console.input_buffer, cmd, CON_IN_BUF_MAX);
            g_console.input_buffer_cursor = ou_strlen(g_console.input_buffer);
        }

        g_console.cmd_history_cursor++;
    }
}

void console_do_history_next() {
    BOOL const any_cmds_in_history = g_console.cmd_history_buffer.count > 0;
    BOOL const cursor_not_at_start = g_console.cmd_history_cursor > 0;

    if (any_cmds_in_history && cursor_not_at_start) {
        SZ const one_idx_before = g_console.cmd_history_cursor - 1;
        SZ const peek_idx       = g_console.cmd_history_buffer.count - one_idx_before;

        C8 const *cmd = ring_get(&g_console.cmd_history_buffer, peek_idx);
        if (cmd) {
            ou_strncpy(g_console.input_buffer, cmd, CON_IN_BUF_MAX);
            g_console.input_buffer_cursor = ou_strlen(g_console.input_buffer);
        }

        g_console.cmd_history_cursor--;
    }

    BOOL const cursor_at_last_cmd_in_history = g_console.cmd_history_cursor == 0;
    if (cursor_at_last_cmd_in_history) {
        g_console.cmd_history_cursor = 0;
        g_console.input_buffer_cursor = 0;
        g_console.input_buffer[0] = '\0';
    }
}

void console_do_move_cursor_begin_line() {
    g_console.input_buffer_cursor = 0;
}

void console_do_move_cursor_end_line() {
    g_console.input_buffer_cursor = ou_strlen(g_console.input_buffer);
}

void console_do_kill_line() {
    // Truncate the string at the cursor position
    g_console.input_buffer[g_console.input_buffer_cursor] = '\0';
}

void console_do_copy() {
    SetClipboardText(g_console.input_buffer);
}

void console_do_paste() {
    C8 const *clip_text = GetClipboardText();
    if (!clip_text || clip_text[0] == '\0') { return; }

    // Calculate available space
    SZ const buffer_length       = ou_strlen(g_console.input_buffer);
    SZ const clip_length         = ou_strlen(clip_text);
    SZ const max_chars_to_insert = CON_IN_BUF_MAX - buffer_length - 1;  // -1 for null terminator

    // If no space left, do nothing
    if (max_chars_to_insert == 0) { return; }

    // Truncate clipboard text
    SZ const chars_to_insert = clip_length > max_chars_to_insert ? max_chars_to_insert : clip_length;

    // Make space for the new text - fix the loop
    // This critical change ensures we don't corrupt the buffer
    for (SZ i = buffer_length; i >= g_console.input_buffer_cursor; --i) {
        // Handle the special case for i=0 to avoid underflow
        if (i == 0 && g_console.input_buffer_cursor == 0) {
            g_console.input_buffer[chars_to_insert] = g_console.input_buffer[0];
            break;
        }
        g_console.input_buffer[i + chars_to_insert] = g_console.input_buffer[i];
    }

    // Insert the clipboard text
    for (SZ i = 0; i < chars_to_insert; ++i) { g_console.input_buffer[g_console.input_buffer_cursor + i] = clip_text[i]; }

    // Ensure null termination
    g_console.input_buffer[buffer_length + chars_to_insert] = '\0';

    // Update cursor position
    g_console.input_buffer_cursor += chars_to_insert;
}

void console_fill_prediction_buffer() {
    // Reset prediction buffer first to avoid stale data.
    g_console.prediction_buffer[0] = '\0';

    // If we have no input, nothing to predict
    if (ou_strlen(g_console.input_buffer) == 0) { return; }

    SZ const input_length = ou_strlen(g_console.input_buffer);

    // Track matches from both commands and CVars
    SZ cmd_match_count   = 0;
    C8 const *cmd_match  = nullptr;
    SZ cvar_match_count  = 0;
    C8 const *cvar_match = nullptr;

    // Check commands first
    for (auto const &i_cmd : i_cmds) {
        if (ou_strncmp(g_console.input_buffer, i_cmd.name, input_length) == 0) {
            cmd_match = i_cmd.name;
            cmd_match_count++;
            if (cmd_match_count > 1) { break; }  // No need to keep counting if we have multiple
        }
    }

    // If exactly one command matches, use it for prediction
    if (cmd_match_count == 1) {
        ou_strncpy(g_console.prediction_buffer, cmd_match, CON_IN_BUF_MAX);
        return;
    }

    // If no unique command match, check CVars
    for (const auto &cvar : cvar_meta_table) {
        if (ou_strncmp(g_console.input_buffer, cvar.name, input_length) == 0) {
            cvar_match = cvar.name;
            cvar_match_count++;
            if (cvar_match_count > 1) { break; }  // No need to keep counting if we have multiple
        }
    }

    // If exactly one CVar matches, use it for prediction
    if (cvar_match_count == 1) { ou_strncpy(g_console.prediction_buffer, cvar_match, CON_IN_BUF_MAX); }

    // If multiple matches of either type, we don't show a prediction
    // (the autocomplete function will handle showing all matches when tab is pressed)
}

// Max number of matches to consider
#define MAX_MATCHES 256

// C-style comparison function for string sorting
S32 static i_str_compare(void const *a, void const *b) {
    C8 const *const *str_a = (C8 const *const *)a;
    C8 const *const *str_b = (C8 const *const *)b;
    return ou_strcmp(*str_a, *str_b);
}

BOOL console_autocomplete_input_buffer() {
    C8 const *cmd_matches[MAX_MATCHES];
    C8 const *cvar_matches[MAX_MATCHES];
    SZ const input_length = ou_strlen(g_console.input_buffer);
    SZ cmd_match_count    = 0;
    SZ cvar_match_count   = 0;
    SZ highlight_length   = input_length;  // New variable to track what we should highlight

    // First check if we have an exact match with a space
    // This indicates the user wants help with command parameters
    for (auto &i_cmd : i_cmds) {
        C8 const *cmd_name_with_space = TS("%s ", i_cmd.name)->c;
        if (ou_strcmp(g_console.input_buffer, cmd_name_with_space) == 0) {
            i_print_help_for_cmd(&i_cmd);
            return true;
        }
    }

    // Look for all partial command matches
    for (auto const &i_cmd : i_cmds) {
        if (cmd_match_count >= MAX_MATCHES) { break; }
        if (ou_strncmp(g_console.input_buffer, i_cmd.name, input_length) == 0) { cmd_matches[cmd_match_count++] = i_cmd.name; }
    }

    // Also check for CVar matches
    for (SZ i = 0; i < CVAR_COUNT && cvar_match_count < MAX_MATCHES; ++i) {
        if (ou_strncmp(g_console.input_buffer, cvar_meta_table[i].name, input_length) == 0) { cvar_matches[cvar_match_count++] = cvar_meta_table[i].name; }
    }

    SZ const total_matches = cmd_match_count + cvar_match_count;
    if (total_matches == 0) { return false; }

    // If there's exactly one match (command or cvar), autocomplete it
    if (total_matches == 1) {
        if (cmd_match_count == 1) {
            ou_strncpy(g_console.input_buffer, cmd_matches[0], CON_IN_BUF_MAX);
        } else {  // cvar_match_count must be 1
            ou_strncpy(g_console.input_buffer, cvar_matches[0], CON_IN_BUF_MAX);
        }
        // Add a space after the match
        ou_strncat(g_console.input_buffer, " ", CON_IN_BUF_MAX - ou_strlen(g_console.input_buffer) - 1);
        return true;
    }

    // Sort command matches
    if (cmd_match_count > 1) { qsort(cmd_matches, cmd_match_count, sizeof(C8 const *), i_str_compare); }

    // Sort cvar matches
    if (cvar_match_count > 1) { qsort(cvar_matches, cvar_match_count, sizeof(C8 const *), i_str_compare); }

    // Multiple matches found - show them by category
    // Reset scroll offset so user can see the suggestions at the bottom
    g_console.vertical_history_offset = 0;
    console_draw_separator();

    // Color definitions for pretty printing
    C8 const *highlight_color = "\\ouc{#ffcc00ff}";  // Yellow for the matching part
    C8 const *category_color  = "\\ouc{#ff9966ff}";   // Orange for category headers
    C8 const *reset_color     = "\\ouc{#ffffffff}";      // White/reset

    // Display match threshold info first
    if (input_length > 0) { lln("%sMatch threshold: %s%zu%s characters", category_color, highlight_color, input_length, reset_color); }

    // Try to find a common prefix and display it (moved here from below)
    C8 const *all_matches[MAX_MATCHES];
    SZ match_idx     = 0;
    SZ prefix_length = input_length;

    // Set up an array with all matches for prefix finding
    for (SZ i = 0; i < cmd_match_count; i++) { all_matches[match_idx++] = cmd_matches[i]; }
    for (SZ i = 0; i < cvar_match_count; i++) { all_matches[match_idx++] = cvar_matches[i]; }

    // Find longest common prefix
    BOOL common_prefix = true;
    while (common_prefix) {
        if (prefix_length >= ou_strlen(all_matches[0])) {
            break;  // Reached the end of the first match
        }
        C8 const next_char = all_matches[0][prefix_length];
        for (SZ i = 1; i < total_matches; ++i) {
            if (prefix_length >= ou_strlen(all_matches[i]) || all_matches[i][prefix_length] != next_char) {
                common_prefix = false;
                break;
            }
        }
        if (common_prefix) {
            ++prefix_length;  // Extend the common prefix length
        }
    }

    // If we found a longer common prefix, show it
    if (prefix_length > input_length) {
        lln("%sCommon prefix: %s%.*s%s", category_color, highlight_color, (S32)prefix_length, all_matches[0], reset_color);

        // Update highlight length to use the new prefix
        highlight_length = prefix_length;

        // Autocomplete to the longest common prefix
        ou_strncpy(g_console.input_buffer, all_matches[0], prefix_length);
        g_console.input_buffer[prefix_length] = '\0';
    }

    // Now show the matches with the updated highlight length
    if (cmd_match_count > 0) {
        C8 const *cmd_color = "\\ouc{#00ccffff}";  // Cyan for commands

        lln("%sPossible matches (commands):%s", category_color, reset_color);
        for (SZ i = 0; i < cmd_match_count; ++i) {
            // Print the matching prefix in highlight color and the rest in cmd color
            lln("- %s%.*s%s%s%s", highlight_color, (S32)highlight_length, cmd_matches[i], cmd_color, &cmd_matches[i][highlight_length], reset_color);
        }
    }

    if (cvar_match_count > 0) {
        C8 const *cvar_color = "\\ouc{#00ff88ff}";  // Green for cvars

        lln("%sPossible matches (cvars):%s", category_color, reset_color);
        for (SZ i = 0; i < cvar_match_count; ++i) {
            // Print the matching prefix in highlight color and the rest in cvar color
            lln("- %s%.*s%s%s%s", highlight_color, (S32)highlight_length, cvar_matches[i], cvar_color, &cvar_matches[i][highlight_length], reset_color);
        }
    }

    // Return whether we found a longer common prefix
    return (prefix_length > input_length);
}

void console_print_to_output(C8 const *message) {
    if (!g_console.initialized) { return; }
    // Duplicate the message on the heap since the callee will free it.
    C8 const *message_copy = ou_strdup(message, MEMORY_TYPE_DARENA);
    if (message_copy == nullptr) { return; }
    ring_push(&g_console.output_buffer, message_copy);
}

void console_draw_separator() {
    ring_push(&g_console.output_buffer, ou_strdup("----", MEMORY_TYPE_DARENA));
}

void console_clear() {
    ring_clear(&g_console.output_buffer);
}
