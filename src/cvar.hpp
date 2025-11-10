#pragma once

#include "common.hpp"

// WARN: DO NOT EDIT - THIS IS A GENERATED FILE!

#define CVAR_COUNT 76
#define CVAR_FILE_NAME "ouro.cvar"
#define CVAR_NAME_MAX_LENGTH 128
#define CVAR_STR_MAX_LENGTH 128

#define CVAR_LONGEST_NAME_LENGTH 38
#define CVAR_LONGEST_VALUE_LENGTH 12
#define CVAR_LONGEST_TYPE_LENGTH 7

struct CVarStr {
    C8 cstr[CVAR_STR_MAX_LENGTH];
};

enum CVarType : U8 {
    CVAR_TYPE_BOOL,
    CVAR_TYPE_S32,
    CVAR_TYPE_F32,
    CVAR_TYPE_CVARSTR,
};

struct CVarMeta {
    C8 name[CVAR_NAME_MAX_LENGTH];
    void *address;
    CVarType type;
    C8 comment[CVAR_NAME_MAX_LENGTH];
};

extern BOOL    c_audio__doppler_enabled;
extern F32     c_audio__doppler_scale;
extern F32     c_audio__max_distance;
extern F32     c_audio__min_distance;
extern F32     c_audio__pan_ambience;
extern F32     c_audio__pan_music;
extern F32     c_audio__pan_sfx;
extern F32     c_audio__pan_voice;
extern F32     c_audio__pitch_ambience;
extern F32     c_audio__pitch_music;
extern F32     c_audio__pitch_sfx;
extern F32     c_audio__pitch_voice;
extern F32     c_audio__rolloff_scale;
extern F32     c_audio__volume_ambience;
extern F32     c_audio__volume_music;
extern F32     c_audio__volume_sfx;
extern F32     c_audio__volume_voice;
extern BOOL    c_console__enabled;
extern CVarStr c_console__font;
extern S32     c_console__font_size;
extern BOOL    c_console__fullscreen;
extern BOOL    c_debug__audio_info;
extern BOOL    c_debug__bbox_info;
extern BOOL    c_debug__bone_info;
extern S32     c_debug__bone_label_font_size;
extern BOOL    c_debug__camera_info;
extern BOOL    c_debug__cursor_info;
extern BOOL    c_debug__dungeon_info;
extern BOOL    c_debug__enabled;
extern BOOL    c_debug__gizmo_info;
extern BOOL    c_debug__grid_info;
extern BOOL    c_debug__keybindings_info;
extern CVarStr c_debug__large_font;
extern S32     c_debug__large_font_size;
extern BOOL    c_debug__light_info;
extern CVarStr c_debug__medium_font;
extern S32     c_debug__medium_font_size;
extern BOOL    c_debug__menu_info;
extern BOOL    c_debug__noclip;
extern F32     c_debug__noclip_move_speed;
extern BOOL    c_debug__profiler_hide_trivial_tracks;
extern BOOL    c_debug__profiler_tracks;
extern BOOL    c_debug__radius_info;
extern CVarStr c_debug__small_font;
extern S32     c_debug__small_font_size;
extern BOOL    c_debug__terrain_info;
extern BOOL    c_debug__texture_widget_dark_bg;
extern BOOL    c_debug__windows_sticky;
extern BOOL    c_profiler__flame_graph_enabled;
extern CVarStr c_profiler__flame_graph_font;
extern S32     c_profiler__flame_graph_font_size;
extern F32     c_profiler__flame_graph_position_y;
extern CVarStr c_profiler__frame_graph_button_font;
extern S32     c_profiler__frame_graph_button_font_size;
extern CVarStr c_profiler__frame_graph_legend_font;
extern S32     c_profiler__frame_graph_legend_font_size;
extern CVarStr c_profiler__frame_graph_y_axis_font;
extern S32     c_profiler__frame_graph_y_axis_font_size;
extern F32     c_render__dungeon_fog_density;
extern BOOL    c_render__fboy;
extern BOOL    c_render__hud;
extern F32     c_render__overworld_fog_density;
extern BOOL    c_render__sketch;
extern BOOL    c_render__skybox;
extern BOOL    c_render__skybox_night;
extern BOOL    c_render__tboy;
extern BOOL    c_video__fps_info;
extern S32     c_video__fps_max;
extern S32     c_video__render_resolution_height;
extern S32     c_video__render_resolution_width;
extern BOOL    c_video__vsync;
extern S32     c_video__window_resolution_height;
extern S32     c_video__window_resolution_width;
extern BOOL    c_world__actor_healthbar;
extern BOOL    c_world__actor_info;
extern BOOL    c_world__verbose_actors;

extern const CVarMeta cvar_meta_table[CVAR_COUNT];

void cvar_save();
