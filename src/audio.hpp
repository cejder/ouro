#pragma once

#include "common.hpp"

#include <raylib.h>
#include <fmod.hpp>
#include <tinycthread.h>

// Need for tinycthread on macOS
#ifdef call_once
#undef call_once
#endif

#define AUDIO_CHANNEL_GROUP_MAX 128
#define AUDIO_VOLUME_DEFAULT_VALUE 0.5F
#define AUDIO_PITCH_DEFAULT_VALUE 1.0F
#define AUDIO_PAN_DEFAULT_VALUE 0.0F
#define AUDIO_MAX_ACTIVE_SOUNDS 1024
#define AUDIO_INVALID_HANDLE 0
#define AUDIO_NAME_MAX_LENGTH 256

#define AUDIO_3D_MIN_DISTANCE 2.5f
#define AUDIO_3D_MAX_DISTANCE 250.0f
#define AUDIO_3D_ROLLOFF_SCALE 1.0f
#define AUDIO_3D_DOPPLER_SCALE 0.1F

#define AUDIO_COMMAND_QUEUE_MAX 16384

using AudioHandle = U32;

enum Audio3DRolloff : U8 {
    AUDIO_3D_ROLLOFF_INVERSE,
    AUDIO_3D_ROLLOFF_LINEAR,
    AUDIO_3D_ROLLOFF_LINEAR_SQUARE,
    AUDIO_3D_ROLLOFF_INVERSE_TAPERED,
    AUDIO_3D_ROLLOFF_CUSTOM,
    AUDIO_3D_ROLLOFF_COUNT
};

enum AudioChannelGroup : U8 {
    ACG_MUSIC,
    ACG_SFX,
    ACG_AMBIENCE,
    ACG_VOICE,
    ACG_COUNT,
};

// Command queue for thread-safe audio playback
enum AudioCommandType : U8 {
    AUDIO_CMD_PLAY_3D_AT_POSITION,
    AUDIO_CMD_PLAY_3D_AT_ENTITY,
    AUDIO_CMD_COUNT
};

struct AudioCommand {
    AudioCommandType type;
    AudioChannelGroup channel_group;
    C8 name[AUDIO_NAME_MAX_LENGTH];
    Vector3 position;  // Used for PLAY_3D_AT_POSITION
    EID entity_id;     // Used for PLAY_3D_AT_ENTITY
};

struct AudioCommandQueue {
    AudioCommand commands[AUDIO_COMMAND_QUEUE_MAX];
    U32 count;
    mtx_t mutex;
};

struct ActiveSound {
    AudioHandle handle;
    FMOD::Channel *channel;
    AudioChannelGroup group;
    C8 name[AUDIO_NAME_MAX_LENGTH];
    BOOL is_3d;
    BOOL is_looping;
    BOOL in_use;
};

struct Audio {
    FMOD::System *fmod_system;
    FMOD::DSP *dsps[ACG_COUNT];
    FMOD::ChannelGroup *groups[ACG_COUNT];

    // Active sound tracking
    ActiveSound active_sounds[AUDIO_MAX_ACTIVE_SOUNDS];
    U32 next_handle;

    // Background music/ambience tracking (for the old API compatibility)
    C8 current_music_name[AUDIO_NAME_MAX_LENGTH];
    C8 current_ambience_name[AUDIO_NAME_MAX_LENGTH];
    AudioHandle current_music_handle;
    AudioHandle current_ambience_handle;

    Audio3DRolloff rolloff_type;
    Vector3 last_listener_position;
    Vector3 listener_velocity;
};

Audio extern g_audio;

// Core audio functions
void audio_init();
Audio *audio_get();
void audio_quit();
FMOD::System *audio_get_fmod_system();
void audio_update(F32 dt);
void audio_reset_all();
void audio_reset(AudioChannelGroup channel_group);
void audio_pause_all();
void audio_pause(AudioChannelGroup channel_group);
void audio_resume_all();
void audio_resume(AudioChannelGroup channel_group);
void audio_stop_all();
void audio_stop(AudioChannelGroup channel_group);
void audio_play(AudioChannelGroup channel_group, C8 const *name);
FMOD::Channel *audio_play_with_channel(AudioChannelGroup channel_group, C8 const *name);
void audio_attach_dsp_by_type(AudioChannelGroup channel_group, FMOD_DSP_TYPE type);
void audio_detach_dsp(AudioChannelGroup channel_group);
void audio_set_volume(AudioChannelGroup channel_group, F32 volume);
void audio_set_pitch(AudioChannelGroup channel_group, F32 pitch);
void audio_set_pan(AudioChannelGroup channel_group, F32 pan);
F32 audio_get_volume(AudioChannelGroup channel_group);
F32 audio_get_pitch(AudioChannelGroup channel_group);
F32 audio_get_pan(AudioChannelGroup channel_group);
F32 *audio_get_volume_ptr(AudioChannelGroup channel_group);
F32 *audio_get_pitch_ptr(AudioChannelGroup channel_group);
F32 *audio_get_pan_ptr(AudioChannelGroup channel_group);
void audio_get_length(C8 const *name, U32 *length);
void audio_add_volume(AudioChannelGroup channel_group, F32 volume);
void audio_add_pitch(AudioChannelGroup channel_group, F32 pitch);
void audio_add_pan(AudioChannelGroup channel_group, F32 pan);
C8 const *audio_get_current_music_name();
C8 const *audio_get_current_ambience_name();
C8 const *audio_channel_group_to_cstr(AudioChannelGroup channel_group);

// 3D audio functions
void audio_3d_init();
void audio_3d_update_listener(F32 dt);
FMOD::Channel *audio_play_3d_at_position(AudioChannelGroup channel_group, C8 const *name, Vector3 position);
FMOD::Channel *audio_play_3d_at_entity(AudioChannelGroup channel_group, C8 const *name, EID entity_id);
void audio_3d_set_position(FMOD::Channel *channel, Vector3 position);
void audio_3d_set_velocity(FMOD::Channel *channel, Vector3 velocity);
void audio_3d_set_min_distance(F32 distance);
void audio_3d_set_max_distance(F32 distance);
void audio_3d_set_rolloff_scale(F32 scale);
void audio_3d_set_rolloff_type(Audio3DRolloff rolloff);
void audio_3d_set_doppler_enabled(BOOL enabled);
void audio_3d_set_doppler_scale(F32 scale);
void audio_draw_3d_dbg();

// Thread-safe command queue API (safe to call from worker threads)
void audio_queue_play_3d_at_position(AudioChannelGroup channel_group, C8 const *name, Vector3 position);
void audio_queue_play_3d_at_entity(AudioChannelGroup channel_group, C8 const *name, EID entity_id);

// Main thread only: process all queued commands
void audio_process_command_queue();
