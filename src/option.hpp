#pragma once

#include "array.hpp"
#include "audio.hpp"
#include "common.hpp"

#include <raylib.h>

fwd_decl(INIFile);

struct Options {
    INIFile *ini;
    Vector2Array available_resolutions;
    BOOL fresh_install;
};

Options extern g_options;

Options *option_get();
void option_init();
void option_set_optimal_settings();
Vector2 option_get_previous_video_resolution();
Vector2 option_get_next_video_resolution();
S32 option_get_previous_video_max_fps();
S32 option_get_next_video_max_fps();
BOOL option_get_audio_enabled(AudioChannelGroup channel_group);
void option_set_game_show_frame_info(BOOL frame_info);
void option_set_video_resolution(Vector2 res);
void option_set_video_max_fps(S32 fps);
void option_set_video_vsync(BOOL vsync);
void option_set_audio_enabled(AudioChannelGroup channel_group, BOOL enabled);
void option_set_audio_volume(AudioChannelGroup channel_group, F32 volume);
void option_update();
