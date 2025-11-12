#include "cvar.hpp"
#include "assert.hpp"
#include "log.hpp"
#include "memory.hpp"
#include "std.hpp"
#include "string.hpp"

#include <glm/common.hpp>
#include <raylib.h>

// WARN: DO NOT EDIT - THIS IS A GENERATED FILE!

BOOL    c_audio__doppler_enabled                 = true;
F32     c_audio__doppler_scale                   = 0.10000000F;
F32     c_audio__max_distance                    = 250.00000000F;
F32     c_audio__min_distance                    = 2.50000000F;
F32     c_audio__pan_ambience                    = 0.00000000F;
F32     c_audio__pan_music                       = 0.00000000F;
F32     c_audio__pan_sfx                         = 0.00000000F;
F32     c_audio__pan_voice                       = 0.00000000F;
F32     c_audio__pitch_ambience                  = 1.00000000F;
F32     c_audio__pitch_music                     = 1.00000000F;
F32     c_audio__pitch_sfx                       = 1.00000000F;
F32     c_audio__pitch_voice                     = 1.63700008F;
F32     c_audio__rolloff_scale                   = 1.00000000F;
F32     c_audio__volume_ambience                 = 0.00000000F;
F32     c_audio__volume_music                    = 0.00000000F;
F32     c_audio__volume_sfx                      = 0.00000000F;
F32     c_audio__volume_voice                    = 0.00000000F;
BOOL    c_console__enabled                       = false;
CVarStr c_console__font                          = {"GoMono"};
S32     c_console__font_size                     = 24;
BOOL    c_console__fullscreen                    = false;
BOOL    c_debug__audio_info                      = false;
BOOL    c_debug__bbox_info                       = false;
BOOL    c_debug__bone_info                       = false;
S32     c_debug__bone_label_font_size            = 24;
BOOL    c_debug__camera_info                     = false;
BOOL    c_debug__cursor_info                     = false;
BOOL    c_debug__dungeon_info                    = true;
BOOL    c_debug__enabled                         = true;
BOOL    c_debug__gizmo_info                      = false;
BOOL    c_debug__grid_info                       = false;
BOOL    c_debug__keybindings_info                = false;
CVarStr c_debug__large_font                      = {"GoMono"};
S32     c_debug__large_font_size                 = 20;
BOOL    c_debug__light_info                      = true;
CVarStr c_debug__medium_font                     = {"GoMono"};
S32     c_debug__medium_font_size                = 18;
BOOL    c_debug__menu_info                       = false;
BOOL    c_debug__noclip                          = false;
F32     c_debug__noclip_move_speed               = 50.00000000F;
BOOL    c_debug__profiler_hide_trivial_tracks    = true;
BOOL    c_debug__profiler_tracks                 = true;
BOOL    c_debug__radius_info                     = false;
CVarStr c_debug__small_font                      = {"GoMono"};
S32     c_debug__small_font_size                 = 16;
BOOL    c_debug__terrain_info                    = false;
BOOL    c_debug__texture_widget_dark_bg          = true;
BOOL    c_debug__windows_sticky                  = false;
BOOL    c_general__multithreaded                 = true;
BOOL    c_profiler__flame_graph_enabled          = true;
CVarStr c_profiler__flame_graph_font             = {"GoMono"};
S32     c_profiler__flame_graph_font_size        = 18;
F32     c_profiler__flame_graph_position_y       = 0.81166267F;
CVarStr c_profiler__frame_graph_button_font      = {"GoMono"};
S32     c_profiler__frame_graph_button_font_size = 22;
CVarStr c_profiler__frame_graph_legend_font      = {"GoMono"};
S32     c_profiler__frame_graph_legend_font_size = 20;
CVarStr c_profiler__frame_graph_y_axis_font      = {"GoMono"};
S32     c_profiler__frame_graph_y_axis_font_size = 16;
F32     c_render__dungeon_fog_density            = 0.03300000F;
BOOL    c_render__fboy                           = false;
BOOL    c_render__hud                            = true;
F32     c_render__overworld_fog_density          = 0.00050000F;
BOOL    c_render__sketch                         = true;
BOOL    c_render__skybox                         = true;
BOOL    c_render__skybox_night                   = true;
BOOL    c_render__tboy                           = false;
BOOL    c_video__fps_info                        = true;
S32     c_video__fps_max                         = 0;
S32     c_video__render_resolution_height        = 2160;
S32     c_video__render_resolution_width         = 3840;
BOOL    c_video__vsync                           = false;
S32     c_video__window_resolution_height        = 2160;
S32     c_video__window_resolution_width         = 3840;
BOOL    c_world__actor_healthbar                 = true;
BOOL    c_world__actor_info                      = false;
BOOL    c_world__verbose_actors                  = false;

CVarMeta const cvar_meta_table[CVAR_COUNT] = {
    {"audio__doppler_enabled",                  &c_audio__doppler_enabled,                  CVAR_TYPE_BOOL,     ""},
    {"audio__doppler_scale",                    &c_audio__doppler_scale,                    CVAR_TYPE_F32,      ""},
    {"audio__max_distance",                     &c_audio__max_distance,                     CVAR_TYPE_F32,      ""},
    {"audio__min_distance",                     &c_audio__min_distance,                     CVAR_TYPE_F32,      ""},
    {"audio__pan_ambience",                     &c_audio__pan_ambience,                     CVAR_TYPE_F32,      ""},
    {"audio__pan_music",                        &c_audio__pan_music,                        CVAR_TYPE_F32,      ""},
    {"audio__pan_sfx",                          &c_audio__pan_sfx,                          CVAR_TYPE_F32,      ""},
    {"audio__pan_voice",                        &c_audio__pan_voice,                        CVAR_TYPE_F32,      ""},
    {"audio__pitch_ambience",                   &c_audio__pitch_ambience,                   CVAR_TYPE_F32,      ""},
    {"audio__pitch_music",                      &c_audio__pitch_music,                      CVAR_TYPE_F32,      ""},
    {"audio__pitch_sfx",                        &c_audio__pitch_sfx,                        CVAR_TYPE_F32,      ""},
    {"audio__pitch_voice",                      &c_audio__pitch_voice,                      CVAR_TYPE_F32,      ""},
    {"audio__rolloff_scale",                    &c_audio__rolloff_scale,                    CVAR_TYPE_F32,      ""},
    {"audio__volume_ambience",                  &c_audio__volume_ambience,                  CVAR_TYPE_F32,      ""},
    {"audio__volume_music",                     &c_audio__volume_music,                     CVAR_TYPE_F32,      ""},
    {"audio__volume_sfx",                       &c_audio__volume_sfx,                       CVAR_TYPE_F32,      ""},
    {"audio__volume_voice",                     &c_audio__volume_voice,                     CVAR_TYPE_F32,      ""},
    {"console__enabled",                        &c_console__enabled,                        CVAR_TYPE_BOOL,     ""},
    {"console__font",                           &c_console__font,                           CVAR_TYPE_CVARSTR,  ""},
    {"console__font_size",                      &c_console__font_size,                      CVAR_TYPE_S32,      ""},
    {"console__fullscreen",                     &c_console__fullscreen,                     CVAR_TYPE_BOOL,     ""},
    {"debug__audio_info",                       &c_debug__audio_info,                       CVAR_TYPE_BOOL,     ""},
    {"debug__bbox_info",                        &c_debug__bbox_info,                        CVAR_TYPE_BOOL,     ""},
    {"debug__bone_info",                        &c_debug__bone_info,                        CVAR_TYPE_BOOL,     ""},
    {"debug__bone_label_font_size",             &c_debug__bone_label_font_size,             CVAR_TYPE_S32,      ""},
    {"debug__camera_info",                      &c_debug__camera_info,                      CVAR_TYPE_BOOL,     ""},
    {"debug__cursor_info",                      &c_debug__cursor_info,                      CVAR_TYPE_BOOL,     ""},
    {"debug__dungeon_info",                     &c_debug__dungeon_info,                     CVAR_TYPE_BOOL,     ""},
    {"debug__enabled",                          &c_debug__enabled,                          CVAR_TYPE_BOOL,     ""},
    {"debug__gizmo_info",                       &c_debug__gizmo_info,                       CVAR_TYPE_BOOL,     ""},
    {"debug__grid_info",                        &c_debug__grid_info,                        CVAR_TYPE_BOOL,     ""},
    {"debug__keybindings_info",                 &c_debug__keybindings_info,                 CVAR_TYPE_BOOL,     ""},
    {"debug__large_font",                       &c_debug__large_font,                       CVAR_TYPE_CVARSTR,  ""},
    {"debug__large_font_size",                  &c_debug__large_font_size,                  CVAR_TYPE_S32,      ""},
    {"debug__light_info",                       &c_debug__light_info,                       CVAR_TYPE_BOOL,     ""},
    {"debug__medium_font",                      &c_debug__medium_font,                      CVAR_TYPE_CVARSTR,  ""},
    {"debug__medium_font_size",                 &c_debug__medium_font_size,                 CVAR_TYPE_S32,      ""},
    {"debug__menu_info",                        &c_debug__menu_info,                        CVAR_TYPE_BOOL,     ""},
    {"debug__noclip",                           &c_debug__noclip,                           CVAR_TYPE_BOOL,     ""},
    {"debug__noclip_move_speed",                &c_debug__noclip_move_speed,                CVAR_TYPE_F32,      ""},
    {"debug__profiler_hide_trivial_tracks",     &c_debug__profiler_hide_trivial_tracks,     CVAR_TYPE_BOOL,     ""},
    {"debug__profiler_tracks",                  &c_debug__profiler_tracks,                  CVAR_TYPE_BOOL,     ""},
    {"debug__radius_info",                      &c_debug__radius_info,                      CVAR_TYPE_BOOL,     ""},
    {"debug__small_font",                       &c_debug__small_font,                       CVAR_TYPE_CVARSTR,  ""},
    {"debug__small_font_size",                  &c_debug__small_font_size,                  CVAR_TYPE_S32,      ""},
    {"debug__terrain_info",                     &c_debug__terrain_info,                     CVAR_TYPE_BOOL,     ""},
    {"debug__texture_widget_dark_bg",           &c_debug__texture_widget_dark_bg,           CVAR_TYPE_BOOL,     ""},
    {"debug__windows_sticky",                   &c_debug__windows_sticky,                   CVAR_TYPE_BOOL,     ""},
    {"general__multithreaded",                  &c_general__multithreaded,                  CVAR_TYPE_BOOL,     ""},
    {"profiler__flame_graph_enabled",           &c_profiler__flame_graph_enabled,           CVAR_TYPE_BOOL,     ""},
    {"profiler__flame_graph_font",              &c_profiler__flame_graph_font,              CVAR_TYPE_CVARSTR,  ""},
    {"profiler__flame_graph_font_size",         &c_profiler__flame_graph_font_size,         CVAR_TYPE_S32,      ""},
    {"profiler__flame_graph_position_y",        &c_profiler__flame_graph_position_y,        CVAR_TYPE_F32,      ""},
    {"profiler__frame_graph_button_font",       &c_profiler__frame_graph_button_font,       CVAR_TYPE_CVARSTR,  ""},
    {"profiler__frame_graph_button_font_size",  &c_profiler__frame_graph_button_font_size,  CVAR_TYPE_S32,      ""},
    {"profiler__frame_graph_legend_font",       &c_profiler__frame_graph_legend_font,       CVAR_TYPE_CVARSTR,  ""},
    {"profiler__frame_graph_legend_font_size",  &c_profiler__frame_graph_legend_font_size,  CVAR_TYPE_S32,      ""},
    {"profiler__frame_graph_y_axis_font",       &c_profiler__frame_graph_y_axis_font,       CVAR_TYPE_CVARSTR,  ""},
    {"profiler__frame_graph_y_axis_font_size",  &c_profiler__frame_graph_y_axis_font_size,  CVAR_TYPE_S32,      ""},
    {"render__dungeon_fog_density",             &c_render__dungeon_fog_density,             CVAR_TYPE_F32,      ""},
    {"render__fboy",                            &c_render__fboy,                            CVAR_TYPE_BOOL,     ""},
    {"render__hud",                             &c_render__hud,                             CVAR_TYPE_BOOL,     ""},
    {"render__overworld_fog_density",           &c_render__overworld_fog_density,           CVAR_TYPE_F32,      ""},
    {"render__sketch",                          &c_render__sketch,                          CVAR_TYPE_BOOL,     ""},
    {"render__skybox",                          &c_render__skybox,                          CVAR_TYPE_BOOL,     ""},
    {"render__skybox_night",                    &c_render__skybox_night,                    CVAR_TYPE_BOOL,     ""},
    {"render__tboy",                            &c_render__tboy,                            CVAR_TYPE_BOOL,     ""},
    {"video__fps_info",                         &c_video__fps_info,                         CVAR_TYPE_BOOL,     ""},
    {"video__fps_max",                          &c_video__fps_max,                          CVAR_TYPE_S32,      ""},
    {"video__render_resolution_height",         &c_video__render_resolution_height,         CVAR_TYPE_S32,      ""},
    {"video__render_resolution_width",          &c_video__render_resolution_width,          CVAR_TYPE_S32,      ""},
    {"video__vsync",                            &c_video__vsync,                            CVAR_TYPE_BOOL,     ""},
    {"video__window_resolution_height",         &c_video__window_resolution_height,         CVAR_TYPE_S32,      ""},
    {"video__window_resolution_width",          &c_video__window_resolution_width,          CVAR_TYPE_S32,      ""},
    {"world__actor_healthbar",                  &c_world__actor_healthbar,                  CVAR_TYPE_BOOL,     ""},
    {"world__actor_info",                       &c_world__actor_info,                       CVAR_TYPE_BOOL,     ""},
    {"world__verbose_actors",                   &c_world__verbose_actors,                   CVAR_TYPE_BOOL,     ""}
};

void cvar_save() {
    String *t = string_create_empty(MEMORY_TYPE_ARENA_TRANSIENT);

    // Calculate the maximum variable name length
    SZ max_var_name_length = 0;
    for (const auto &cvar : cvar_meta_table) {
        C8 const *sep = ou_strstr(cvar.name, "__");
        if (sep) {
            SZ const var_name_len = ou_strlen(sep + 2);
            max_var_name_length = glm::max(var_name_len, max_var_name_length);
        }
    }

    C8 current_header[CVAR_NAME_MAX_LENGTH] = "";

    for (const auto &cvar : cvar_meta_table) {
        // Split variable name
        C8 const *sep = ou_strstr(cvar.name, "__");
        if (!sep) {
            lle("cvar name %s missing 2x underscore separator", cvar.name);
            continue;
        }

        // Extract header name
        SZ const header_len = (SZ)(sep - cvar.name);
        C8 header[CVAR_NAME_MAX_LENGTH];
        ou_strncpy(header, cvar.name, header_len);
        header[header_len] = '\0';

        // Extract variable name and get its length
        C8 const *var_name = sep + 2;
        SZ const var_name_len = ou_strlen(var_name);

        // Write header if it changed
        if (ou_strcmp(current_header, header) != 0) {
            if (current_header[0] != '\0') {
                string_append(t, "\n"); // Add blank line between sections
            }
            string_appendf(t, "[%s]\n", header);
            ou_strcpy(current_header, header);
        }

        // Write variable name and padding
        string_appendf(t, "%s", var_name);
        string_appendf(t, "%*.s : ", (S32)(max_var_name_length - var_name_len), " ");

        switch (cvar.type) {
            case CVAR_TYPE_BOOL: {
                string_append(t, BOOL_TO_STR(*(BOOL *)(cvar.address)));
            } break;
            case CVAR_TYPE_S32: {
                string_appendf(t, "%d", *(S32 *)(cvar.address));
            } break;
            case CVAR_TYPE_F32: {
                string_appendf(t, "%.8f", *(F32 *)(cvar.address));
            } break;
            case CVAR_TYPE_CVARSTR: {
                string_appendf(t, "%s", ((CVarStr *)(cvar.address))->cstr);
            } break;
            default: {
                _unimplemented_();
            }
        }

        // Add comment if present
        if (cvar.comment[0] != '\0') {
            string_appendf(t, " # %s", cvar.comment);
        }
        string_append(t, "\n");
    }

    if (!SaveFileText(CVAR_FILE_NAME, t->c)) { lle("could not save cvars to %s", CVAR_FILE_NAME); }
}
