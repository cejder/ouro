#include "audio.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "cvar.hpp"
#include "log.hpp"
#include "math.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "std.hpp"
#include "string.hpp"
#include "time.hpp"
#include "world.hpp"

#include <fmod_errors.h>
#include <raymath.h>

Audio g_audio = {};

C8 static const *i_channel_group_names[ACG_COUNT] = {"Music", "SFX", "Ambience", "Voice"};

// Internal state tracking to avoid redundant FMOD calls
struct AudioState {
    F32 last_volume[ACG_COUNT];
    F32 last_pitch[ACG_COUNT];
    F32 last_pan[ACG_COUNT];
    F32 last_dt_mod;
    BOOL needs_update[ACG_COUNT];
} static i_state = {};

// Helper macros for FMOD error checking
#define FC(call, msg)                              \
    do {                                                   \
        FMOD_RESULT const _result = (call);                \
        if (_result != FMOD_OK) {                          \
            lle("%s: %s", msg, FMOD_ErrorString(_result)); \
            return;                                        \
        }                                                  \
    } while (0)

#define FCR(call, msg, ret)                  \
    do {                                                   \
        FMOD_RESULT const _result = (call);                \
        if (_result != FMOD_OK) {                          \
            lle("%s: %s", msg, FMOD_ErrorString(_result)); \
            return ret;                                    \
        }                                                  \
    } while (0)

U32 static i_parse_message(String *s) {
    _assert_(s->length > 0, "Expected a non-empty string");

    C8 c = string_pop_back(s);
    _assert_(c == ')', "Expected a closing parenthesis");

    S32 digits[10] = {};
    SZ digit_count = 0;

    do {
        c = string_pop_back(s);
        if (c == '(') { break; }
        if (c >= '0' && c <= '9') { digits[digit_count++] = c - '0'; }
    } while (!string_empty(s));

    if (c != '(') { lle("expected an opening parenthesis"); }
    if (digit_count == 0) { lle("expected at least one digit"); }

    U32 line_number = 0;
    for (SZ i = 0; i < digit_count; ++i) { line_number += (U32)((F32)digits[digit_count - i - 1] * math_pow_f32(10.0F, (F32)(digit_count - i - 1))); }

    while (string_peek_front(s) == '.' || string_peek_front(s) == '/') { string_pop_front(s); }

    return line_number;
}

void static *i_fmod_malloc(U32 size, FMOD_MEMORY_TYPE type, C8 const *src) {
    unused(type);
    String *path          = TS("%s", src);
    U32 const line_number = i_parse_message(path);
    void *ptr             = malloc(size);  // NOLINT
    llog_format(LLOG_LEVEL_TRACE, path->c, "unknown", line_number, "Allocating memory of size %d at %p", size, ptr);
    return ptr;
}

void static *i_fmod_realloc(void *ptr, U32 size, FMOD_MEMORY_TYPE type, C8 const *src) {
    unused(type);
    String *path          = TS("%s", src);
    U32 const line_number = i_parse_message(path);
    void *new_ptr         = realloc(ptr, size);  // NOLINT
    llog_format(LLOG_LEVEL_TRACE, path->c, "unknown", line_number, "Reallocating memory of size %d at %p", size, new_ptr);
    return new_ptr;
}

void static i_fmod_free(void *ptr, FMOD_MEMORY_TYPE type, C8 const *src) {
    unused(type);
    String *path          = TS("%s", src);
    U32 const line_number = i_parse_message(path);
    llog_format(LLOG_LEVEL_TRACE, path->c, "unknown", line_number, "Freeing memory at %p", ptr);
    free(ptr);  // NOLINT
}

F32 static *i_acg_volume_ptr[ACG_COUNT] = {
    &c_audio__volume_music,
    &c_audio__volume_sfx,
    &c_audio__volume_ambience,
    &c_audio__volume_voice,
};

F32 static *i_acg_pitch_ptr[ACG_COUNT] = {
    &c_audio__pitch_music,
    &c_audio__pitch_sfx,
    &c_audio__pitch_ambience,
    &c_audio__pitch_voice,
};

F32 static *i_acg_pan_ptr[ACG_COUNT] = {
    &c_audio__pan_music,
    &c_audio__pan_sfx,
    &c_audio__pan_ambience,
    &c_audio__pan_voice,
};

AudioHandle static i_generate_handle() {
    return ++g_audio.next_handle;
}

ActiveSound static *i_find_free_slot() {
    for (auto &active_sound : g_audio.active_sounds) {
        if (!active_sound.in_use) { return &active_sound; }
    }
    return nullptr;
}

ActiveSound static *i_find_sound_by_handle(AudioHandle handle) {
    if (handle == AUDIO_INVALID_HANDLE) { return nullptr; }

    for (auto &active_sound : g_audio.active_sounds) {
        if (active_sound.in_use && active_sound.handle == handle) { return &active_sound; }
    }
    return nullptr;
}

void static i_cleanup_finished_sounds() {
    for (auto &active_sound : g_audio.active_sounds) {
        ActiveSound *sound = &active_sound;
        if (!sound->in_use) { continue; }

        BOOL is_playing = false;
        if (sound->channel) {
            FMOD_RESULT const result = sound->channel->isPlaying(&is_playing);
            if (result != FMOD_OK || !is_playing) {
                // Sound finished, clean up
                sound->in_use  = false;
                sound->channel = nullptr;

                // Clear background music/ambience tracking if this was it
                if (sound->handle == g_audio.current_music_handle) {
                    g_audio.current_music_handle = AUDIO_INVALID_HANDLE;
                    ou_memset(g_audio.current_music_name, 0, AUDIO_NAME_MAX_LENGTH);
                }
                if (sound->handle == g_audio.current_ambience_handle) {
                    g_audio.current_ambience_handle = AUDIO_INVALID_HANDLE;
                    ou_memset(g_audio.current_ambience_name, 0, AUDIO_NAME_MAX_LENGTH);
                }
            }
        }
    }
}

void static i_mark_group_for_update(AudioChannelGroup channel_group) {
    i_state.needs_update[channel_group] = true;
}

void static i_update_group_properties(AudioChannelGroup channel_group) {
    if (!i_state.needs_update[channel_group]) { return; }

    F32 const volume     = *i_acg_volume_ptr[channel_group];
    F32 const base_pitch = *i_acg_pitch_ptr[channel_group];
    F32 const pan        = *i_acg_pan_ptr[channel_group];

    // Apply time modification to pitch
    F32 const final_pitch = base_pitch * math_abs_f32(i_state.last_dt_mod);

    FMOD::ChannelGroup *group = g_audio.groups[channel_group];

    FC(group->setVolume(volume), "Could not set volume");
    FC(group->setPitch(final_pitch), "Could not set pitch");
    FC(group->setPan(pan), "Could not set pan");

    // Update cached values
    i_state.last_volume[channel_group]  = volume;
    i_state.last_pitch[channel_group]   = base_pitch;
    i_state.last_pan[channel_group]     = pan;
    i_state.needs_update[channel_group] = false;
}

AudioHandle static i_play_sound_internal(AudioChannelGroup channel_group, C8 const *name, BOOL is_3d, BOOL loop, Vector3 position = {0, 0, 0}) {
    ActiveSound *slot = i_find_free_slot();
    if (!slot) {
        llt("No free audio slots available");
        return AUDIO_INVALID_HANDLE;
    }

    FMOD::Channel *channel = nullptr;
    // Start paused to set up attributes
    FMOD_RESULT const result = g_audio.fmod_system->playSound(asset_get_sound(name)->base, g_audio.groups[channel_group], true, &channel);
    if (result != FMOD_OK || !channel) {
        llw("Could not play sound %s: %s", name, FMOD_ErrorString(result));
        return AUDIO_INVALID_HANDLE;
    }

    // Set up the sound mode
    FMOD_MODE mode = is_3d ? FMOD_3D : FMOD_2D;
    if (loop) { mode |= FMOD_LOOP_NORMAL; }

    FCR(channel->setMode(mode), "Could not set sound mode", AUDIO_INVALID_HANDLE);

    // For 3D sounds, set position and distance settings
    if (is_3d) {
        FMOD_VECTOR const pos = {position.x, position.y, -position.z};  // Convert coordinate system
        FMOD_VECTOR const vel = {0, 0, 0};
        FCR(channel->set3DAttributes(&pos, &vel), "Could not set 3D attributes", AUDIO_INVALID_HANDLE);
        FCR(channel->set3DMinMaxDistance(c_audio__min_distance, c_audio__max_distance), "Could not set 3D min/max distance", AUDIO_INVALID_HANDLE);

        // Set rolloff mode
        FMOD_MODE rolloff_mode = FMOD_DEFAULT;
        switch (g_audio.rolloff_type) {
            case AUDIO_3D_ROLLOFF_INVERSE: {
                rolloff_mode = FMOD_3D_INVERSEROLLOFF;
            } break;
            case AUDIO_3D_ROLLOFF_LINEAR: {
                rolloff_mode = FMOD_3D_LINEARROLLOFF;
            } break;
            case AUDIO_3D_ROLLOFF_LINEAR_SQUARE: {
                rolloff_mode = FMOD_3D_LINEARSQUAREROLLOFF;
            } break;
            case AUDIO_3D_ROLLOFF_INVERSE_TAPERED: {
                rolloff_mode = FMOD_3D_INVERSETAPEREDROLLOFF;
            } break;
            case AUDIO_3D_ROLLOFF_CUSTOM: {
                rolloff_mode = FMOD_3D_CUSTOMROLLOFF;
            } break;
            default: {
                rolloff_mode = FMOD_3D_INVERSEROLLOFF;
            } break;
        }
        FCR(channel->setMode(mode | rolloff_mode), "Could not set rolloff mode", AUDIO_INVALID_HANDLE);
    }

    // Resume playback
    FCR(channel->setPaused(false), "Could not unpause sound", AUDIO_INVALID_HANDLE);

    // Fill in the slot
    slot->handle     = i_generate_handle();
    slot->channel    = channel;
    slot->group      = channel_group;
    slot->is_3d      = is_3d;
    slot->is_looping = loop;
    slot->in_use     = true;
    ou_strncpy(slot->name, name, sizeof(slot->name) - 1);

    if (is_3d) {
        llt("Playing 3D sound '%s' at position (%.2f, %.2f, %.2f) (handle %u)", name, position.x, position.y, position.z, slot->handle);
    } else {
        llt("Playing 2D sound '%s' (handle %u)", name, slot->handle);
    }

    return slot->handle;
}

void audio_init() {
    // Initialize active sounds array
    for (auto &active_sound : g_audio.active_sounds) {
        active_sound.in_use  = false;
        active_sound.handle  = AUDIO_INVALID_HANDLE;
        active_sound.channel = nullptr;
    }
    g_audio.next_handle             = 1;  // Start from 1, 0 is invalid
    g_audio.current_music_handle    = AUDIO_INVALID_HANDLE;
    g_audio.current_ambience_handle = AUDIO_INVALID_HANDLE;

    // Initialize state
    for (S32 i = 0; i < ACG_COUNT; ++i) {
        i_state.last_volume[i]  = -1.0F;  // Force initial update
        i_state.last_pitch[i]   = -1.0F;
        i_state.last_pan[i]     = -999.0F;
        i_state.needs_update[i] = true;

        // Initialize DSP pointers
        g_audio.dsps[i] = nullptr;
    }
    i_state.last_dt_mod = 1.0F;

    // Set up FMOD debug level
    FMOD_DEBUG_FLAGS fmod_level = FMOD_DEBUG_LEVEL_NONE;
    switch (llog_get_level()) {
        case LLOG_LEVEL_TTY:
        case LLOG_LEVEL_NONE: {
            fmod_level = FMOD_DEBUG_LEVEL_NONE;
        } break;
        case LLOG_LEVEL_TRACE:
        case LLOG_LEVEL_DEBUG:
        case LLOG_LEVEL_INFO: {
            fmod_level = FMOD_DEBUG_LEVEL_LOG;
        } break;
        case LLOG_LEVEL_WARN: {
            fmod_level = FMOD_DEBUG_LEVEL_WARNING;
        } break;
        case LLOG_LEVEL_ERROR:
        case LLOG_LEVEL_FATAL: {
            fmod_level = FMOD_DEBUG_LEVEL_ERROR;
        } break;
        default: {
            _unreachable_();
        }
    }

    FC(FMOD::Debug_Initialize(fmod_level, FMOD_DEBUG_MODE_CALLBACK, llog_fmod_cb, nullptr), "Could not initialize FMOD debugging");
    FC(FMOD::Memory_Initialize(nullptr, 0, i_fmod_malloc, i_fmod_realloc, i_fmod_free, FMOD_MEMORY_ALL), "Could not initialize FMOD memory");
    FC(FMOD::System_Create(&g_audio.fmod_system, FMOD_VERSION), "Could not create FMOD system");

    FC(g_audio.fmod_system->init(AUDIO_CHANNEL_GROUP_MAX, FMOD_INIT_NORMAL, nullptr), "Could not init FMOD system");

    // Create channel groups
    for (SZ i = 0; i < ACG_COUNT; ++i) {
        FC(g_audio.fmod_system->createChannelGroup(i_channel_group_names[i], &g_audio.groups[i]), TS("Could not create FMOD channel group %s", i_channel_group_names[i])->c);
    }

    // Initialize track names
    ou_memset(g_audio.current_music_name, 0, AUDIO_NAME_MAX_LENGTH);
    ou_memset(g_audio.current_ambience_name, 0, AUDIO_NAME_MAX_LENGTH);

    audio_3d_init();

    // Apply initial volume settings immediately to prevent audio on first frame
    for (SZ i = 0; i < ACG_COUNT; ++i) { i_update_group_properties((AudioChannelGroup)i); }
}

void audio_quit() {
    // Clean up DSPs first - must remove from channel groups before releasing
    for (SZ i = 0; i < ACG_COUNT; ++i) {
        if (g_audio.dsps[i]) {
            // Remove DSP from channel group first
            if (g_audio.groups[i]) {
                FMOD_RESULT const remove_result = g_audio.groups[i]->removeDSP(g_audio.dsps[i]);
                if (remove_result != FMOD_OK) {
                    lle("Could not remove DSP from channel group %s: %s", i_channel_group_names[i], FMOD_ErrorString(remove_result));
                }
            }
            // Now we can safely release the DSP
            FMOD_RESULT const release_result = g_audio.dsps[i]->release();
            if (release_result != FMOD_OK) {
                lle("Could not release DSP for channel group %s: %s", i_channel_group_names[i], FMOD_ErrorString(release_result));
            }
            g_audio.dsps[i] = nullptr;
        }
    }

    // Release channel groups
    for (SZ i = 0; i < ACG_COUNT; ++i) {
        if (g_audio.groups[i]) { FC(g_audio.groups[i]->release(), TS("Could not release FMOD channelgroup %s", i_channel_group_names[i])->c); }
    }

    if (g_audio.fmod_system) {
        FC(g_audio.fmod_system->close(), "Could not close FMOD system");
        FC(g_audio.fmod_system->release(), "Could not release FMOD system");
    }
}

FMOD::System *audio_get_fmod_system() {
    return g_audio.fmod_system;
}

void audio_update(F32 dt) {
    FC(g_audio.fmod_system->update(), "Could not update FMOD system");

    // Clean up finished sounds
    i_cleanup_finished_sounds();

    // Check if time scaling changed
    F32 const current_dt_mod = time_get_delta_mod();
    if (i_state.last_dt_mod != current_dt_mod) {
        i_state.last_dt_mod = current_dt_mod;
        // Mark all groups for pitch update
        for (S32 i = 0; i < ACG_COUNT; ++i) { i_mark_group_for_update((AudioChannelGroup)i); }
    }

    // Check for cvar changes and update groups as needed
    for (S32 i = 0; i < ACG_COUNT; ++i) {
        auto channel_group = (AudioChannelGroup)i;

        F32 const volume = *i_acg_volume_ptr[channel_group];
        F32 const pitch  = *i_acg_pitch_ptr[channel_group];
        F32 const pan    = *i_acg_pan_ptr[channel_group];

        if (volume != i_state.last_volume[i] || pitch != i_state.last_pitch[i] || pan != i_state.last_pan[i]) { i_mark_group_for_update(channel_group); }
    }

    // Apply updates only to groups that need them
    for (S32 i = 0; i < ACG_COUNT; ++i) { i_update_group_properties((AudioChannelGroup)i); }

    // Handle scene-specific ambience volume
    if (g_scenes.current_scene_type == SCENE_OVERWORLD) {
        F32 const max_height = 400.0F;
        Camera3D const camera = c3d_get();

        F32 volume = 0.15F - (camera.position.y / max_height);
        volume = glm::clamp(volume, 0.001F, 0.15F);  // Keep above 0 but clamp to valid range

        if (audio_get_volume(ACG_AMBIENCE) > 0.0F) { audio_set_volume(ACG_AMBIENCE, volume); }
    }

    audio_3d_update_listener(dt);
}

void audio_reset_all() {
    for (S32 i = 0; i < ACG_COUNT; ++i) {
        audio_reset((AudioChannelGroup)i);
        audio_stop((AudioChannelGroup)i);
    }
}

void audio_reset(AudioChannelGroup channel_group) {
    *i_acg_pitch_ptr[channel_group] = 1.0F;
    *i_acg_pan_ptr[channel_group]   = 0.0F;

    // Clean up DSP
    if (g_audio.dsps[channel_group]) {
        FC(g_audio.groups[channel_group]->removeDSP(g_audio.dsps[channel_group]), "Could not remove DSP");
        g_audio.dsps[channel_group]->release();
        g_audio.dsps[channel_group] = nullptr;
    }
}

void audio_pause_all() {
    for (auto &group : g_audio.groups) { FC(group->setPaused(true), "Could not pause channel group"); }
}

void audio_pause(AudioChannelGroup channel_group) {
    FC(g_audio.groups[channel_group]->setPaused(true), "Could not pause channel group");
}

void audio_resume_all() {
    for (auto &group : g_audio.groups) { FC(group->setPaused(false), "Could not resume channel group"); }
}

void audio_resume(AudioChannelGroup channel_group) {
    FC(g_audio.groups[channel_group]->setPaused(false), "Could not resume channel group");
}

void audio_stop_all() {
    for (S32 i = 0; i < ACG_COUNT; ++i) { FC(g_audio.groups[i]->stop(), TS("Could not stop FMOD channelgroup %s", i_channel_group_names[i])->c); }
    ou_memset(g_audio.current_music_name, 0, AUDIO_NAME_MAX_LENGTH);
    ou_memset(g_audio.current_ambience_name, 0, AUDIO_NAME_MAX_LENGTH);
    g_audio.current_music_handle    = AUDIO_INVALID_HANDLE;
    g_audio.current_ambience_handle = AUDIO_INVALID_HANDLE;

    // Mark all active sounds as not in use
    for (auto &active_sound : g_audio.active_sounds) {
        active_sound.in_use = false;
        active_sound.channel = nullptr;
    }
}

void audio_stop(AudioChannelGroup channel_group) {
    FC(g_audio.groups[channel_group]->stop(), TS("Could not stop FMOD channelgroup %s", i_channel_group_names[channel_group])->c);

    // Clean up tracking for this channel group
    for (auto &active_sound : g_audio.active_sounds) {
        if (active_sound.in_use && active_sound.group == channel_group) {
            active_sound.in_use  = false;
            active_sound.channel = nullptr;
        }
    }

    if (channel_group == ACG_MUSIC) {
        ou_memset(g_audio.current_music_name, 0, AUDIO_NAME_MAX_LENGTH);
        g_audio.current_music_handle = AUDIO_INVALID_HANDLE;
    }
    if (channel_group == ACG_AMBIENCE) {
        ou_memset(g_audio.current_ambience_name, 0, AUDIO_NAME_MAX_LENGTH);
        g_audio.current_ambience_handle = AUDIO_INVALID_HANDLE;
    }
}

void audio_play(AudioChannelGroup channel_group, C8 const *name) {
    audio_resume(channel_group);

    BOOL const is_music    = channel_group == ACG_MUSIC;
    BOOL const is_ambience = channel_group == ACG_AMBIENCE;

    // For background music/ambience, stop previous if different
    if (is_music) {
        if (ou_strcmp(name, g_audio.current_music_name) == 0) {
            llt("Already playing music (%s)", g_audio.current_music_name);
            return;
        }
        if (g_audio.current_music_handle != AUDIO_INVALID_HANDLE) {
            llt("Stopping current music (%s) to play %s", g_audio.current_music_name, name);
            audio_stop(ACG_MUSIC);
        }
    }

    if (is_ambience) {
        if (ou_strcmp(name, g_audio.current_ambience_name) == 0) {
            llt("Already playing ambience (%s)", g_audio.current_ambience_name);
            return;
        }
        if (g_audio.current_ambience_handle != AUDIO_INVALID_HANDLE) {
            llt("Stopping current ambience (%s) to play %s", g_audio.current_ambience_name, name);
            audio_stop(ACG_AMBIENCE);
        }
    }

    // Play the sound using the new system
    BOOL const should_loop   = is_music || is_ambience;
    AudioHandle const handle = i_play_sound_internal(channel_group, name, false, should_loop);

    if (handle != AUDIO_INVALID_HANDLE) {
        if (is_music) {
            ou_strncpy(g_audio.current_music_name, name, AUDIO_NAME_MAX_LENGTH - 1);
            g_audio.current_music_handle = handle;
            llt("Now playing music: %s", name);
        } else if (is_ambience) {
            ou_strncpy(g_audio.current_ambience_name, name, AUDIO_NAME_MAX_LENGTH - 1);
            g_audio.current_ambience_handle = handle;
            llt("Now playing ambience: %s", name);
        }
    }
}

FMOD::Channel *audio_play_with_channel(AudioChannelGroup channel_group, C8 const *name) {
    audio_resume(channel_group);

    // Simple version for SFX - just play and return the channel
    AudioHandle const handle = i_play_sound_internal(channel_group, name, false, false);
    if (handle == AUDIO_INVALID_HANDLE) { return nullptr; }

    ActiveSound *sound = i_find_sound_by_handle(handle);
    return sound ? sound->channel : nullptr;
}

void audio_attach_dsp_by_type(AudioChannelGroup channel_group, FMOD_DSP_TYPE type) {
    // Clean up existing DSP first
    if (g_audio.dsps[channel_group]) {
        g_audio.groups[channel_group]->removeDSP(g_audio.dsps[channel_group]);
        g_audio.dsps[channel_group]->release();
    }

    FC(g_audio.fmod_system->createDSPByType(type, &g_audio.dsps[channel_group]), TS("Could not create DSP (%d)", type)->c);

    FC(g_audio.groups[channel_group]->addDSP(0, g_audio.dsps[channel_group]), TS("Could not add DSP (%d) to channel group", type)->c);

    llt("Effect (%d) added to channel group %s", type, i_channel_group_names[channel_group]);
}

void audio_detach_dsp(AudioChannelGroup channel_group) {
    if (!g_audio.dsps[channel_group]) { return; }

    FC(g_audio.groups[channel_group]->removeDSP(g_audio.dsps[channel_group]), "Could not remove DSP");

    g_audio.dsps[channel_group]->release();
    g_audio.dsps[channel_group] = nullptr;

    llt("Effect removed from channel group %s", i_channel_group_names[channel_group]);
}

void audio_set_volume(AudioChannelGroup channel_group, F32 volume) {
    *i_acg_volume_ptr[channel_group] = volume;
    i_mark_group_for_update(channel_group);
}

void audio_set_pitch(AudioChannelGroup channel_group, F32 pitch) {
    *i_acg_pitch_ptr[channel_group] = pitch;
    i_mark_group_for_update(channel_group);
}

void audio_set_pan(AudioChannelGroup channel_group, F32 pan) {
    *i_acg_pan_ptr[channel_group] = pan;
    i_mark_group_for_update(channel_group);
}

F32 audio_get_volume(AudioChannelGroup channel_group) {
    return *i_acg_volume_ptr[channel_group];
}

F32 audio_get_pitch(AudioChannelGroup channel_group) {
    return *i_acg_pitch_ptr[channel_group];
}

F32 audio_get_pan(AudioChannelGroup channel_group) {
    return *i_acg_pan_ptr[channel_group];
}

F32 *audio_get_volume_ptr(AudioChannelGroup channel_group) {
    return i_acg_volume_ptr[channel_group];
}

F32 *audio_get_pitch_ptr(AudioChannelGroup channel_group) {
    return i_acg_pitch_ptr[channel_group];
}

F32 *audio_get_pan_ptr(AudioChannelGroup channel_group) {
    return i_acg_pan_ptr[channel_group];
}

void audio_get_length(C8 const *name, U32 *length) {
    FC(asset_get_sound(name)->base->getLength(length, FMOD_TIMEUNIT_MS), "Could not get sound length");
}

void audio_add_volume(AudioChannelGroup channel_group, F32 volume) {
    F32 *ptr = i_acg_volume_ptr[channel_group];
    *ptr     = glm::clamp(*ptr + volume, 0.0F, 1.0F);
    i_mark_group_for_update(channel_group);
}

void audio_add_pitch(AudioChannelGroup channel_group, F32 pitch) {
    F32 *ptr = i_acg_pitch_ptr[channel_group];
    *ptr     = glm::clamp(*ptr + pitch, 0.5F, 2.0F);
    i_mark_group_for_update(channel_group);
}

void audio_add_pan(AudioChannelGroup channel_group, F32 pan) {
    F32 *ptr = i_acg_pan_ptr[channel_group];
    *ptr     = glm::clamp(*ptr + pan, -1.0F, 1.0F);
    i_mark_group_for_update(channel_group);
}

C8 const *audio_get_current_music_name() {
    return g_audio.current_music_name[0] != '\0' ? g_audio.current_music_name : "None";
}

C8 const *audio_get_current_ambience_name() {
    return g_audio.current_ambience_name[0] != '\0' ? g_audio.current_ambience_name : "None";
}

C8 const *audio_channel_group_to_cstr(AudioChannelGroup channel_group) {
    return i_channel_group_names[channel_group];
}

void audio_3d_init() {
    g_audio.rolloff_type           = AUDIO_3D_ROLLOFF_INVERSE;
    g_audio.last_listener_position = {0, 0, 0};
    g_audio.listener_velocity      = {0, 0, 0};
    FC(g_audio.fmod_system->set3DSettings(c_audio__doppler_scale, 1.0F, c_audio__rolloff_scale), "Could not set FMOD 3D settings");
}

void audio_3d_update_listener(F32 dt) {
    Camera3D const camera = c3d_get();

    // Calculate listener velocity for doppler
    Vector3 const current_pos = camera.position;

    if (dt > 0.0F) {
        Vector3 const velocity = {
            (current_pos.x - g_audio.last_listener_position.x) / dt,
            (current_pos.y - g_audio.last_listener_position.y) / dt,
            (current_pos.z - g_audio.last_listener_position.z) / dt,
        };
        g_audio.listener_velocity = velocity;
    }

    // Calculate and normalize forward vector
    Vector3 forward          = Vector3Subtract(camera.target, camera.position);
    F32 const forward_length = Vector3Length(forward);

    // Safety check for zero-length forward vector
    if (forward_length < 0.001F) {
        forward = (Vector3){0.0F, 0.0F, -1.0F};
    } else {
        forward = Vector3Scale(forward, 1.0F / forward_length);
    }

    // Ensure up vector is normalized
    Vector3 up = Vector3Normalize(camera.up);

    // Make sure up is perpendicular to forward using Gram-Schmidt process
    Vector3 right          = Vector3CrossProduct(forward, up);
    F32 const right_length = Vector3Length(right);

    if (right_length < 0.001F) {
        if (math_abs_f32(forward.y) < 0.9F) {
            right = Vector3CrossProduct(forward, (Vector3){0.0F, 1.0F, 0.0F});
        } else {
            right = Vector3CrossProduct(forward, (Vector3){1.0F, 0.0F, 0.0F});
        }
        right = Vector3Normalize(right);
    } else {
        right = Vector3Scale(right, 1.0F / right_length);
    }

    // Recalculate up to ensure it's perpendicular to both forward and right
    up = Vector3CrossProduct(right, forward);
    up = Vector3Normalize(up);

    // Convert to FMOD vectors (coordinate system conversion)
    FMOD_VECTOR const pos = {camera.position.x, camera.position.y, -camera.position.z};
    FMOD_VECTOR const vel = {g_audio.listener_velocity.x, g_audio.listener_velocity.y, -g_audio.listener_velocity.z};
    FMOD_VECTOR const fwd = {forward.x, forward.y, -forward.z};
    FMOD_VECTOR const upv = {up.x, up.y, -up.z};

    FC(g_audio.fmod_system->set3DListenerAttributes(0, &pos, &vel, &fwd, &upv), "Could not update 3D listener position");

    g_audio.last_listener_position = current_pos;
}

FMOD::Channel *audio_play_3d_at_position(AudioChannelGroup channel_group, C8 const *name, Vector3 position) {
    AudioHandle const handle = i_play_sound_internal(channel_group, name, true, false, position);
    if (handle == AUDIO_INVALID_HANDLE) { return nullptr; }

    ActiveSound *sound = i_find_sound_by_handle(handle);
    return sound ? sound->channel : nullptr;
}

FMOD::Channel *audio_play_3d_at_entity(AudioChannelGroup channel_group, C8 const *name, EID entity_id) {
    if (!entity_is_valid(entity_id)) {
        llw("Cannot play 3D sound at invalid entity %u", entity_id);
        return nullptr;
    }

    return audio_play_3d_at_position(channel_group, name, g_world->position[entity_id]);
}

void audio_3d_set_position(FMOD::Channel *channel, Vector3 position) {
    if (!channel) { return; }

    FMOD_VECTOR const pos = {position.x, position.y, -position.z};
    FMOD_VECTOR const vel = {0, 0, 0};

    FC(channel->set3DAttributes(&pos, &vel), "Could not update 3D sound position");
}

void audio_3d_set_velocity(FMOD::Channel *channel, Vector3 velocity) {
    if (!channel) { return; }

    FMOD_VECTOR pos;
    FMOD_VECTOR vel;
    channel->get3DAttributes(&pos, &vel);

    vel = {velocity.x, velocity.y, -velocity.z};
    FC(channel->set3DAttributes(&pos, &vel), "Could not update 3D sound velocity");
}

// Settings functions
void audio_3d_set_min_distance(F32 distance) {
    c_audio__min_distance = distance;
}

void audio_3d_set_max_distance(F32 distance) {
    c_audio__max_distance = distance;
}

void audio_3d_set_rolloff_scale(F32 scale) {
    c_audio__rolloff_scale = scale;
    FC(g_audio.fmod_system->set3DSettings(c_audio__doppler_scale, 1.0F, scale), "Could not update 3D rolloff scale");
}

void audio_3d_set_rolloff_type(Audio3DRolloff rolloff) {
    g_audio.rolloff_type = rolloff;
}

void audio_3d_set_doppler_enabled(BOOL enabled) {
    c_audio__doppler_enabled = enabled;
    F32 const doppler_scale = enabled ? c_audio__doppler_scale : 0.0F;
    FC(g_audio.fmod_system->set3DSettings(doppler_scale, 1.0F, c_audio__rolloff_scale), "Could not update doppler settings");
}

void audio_3d_set_doppler_scale(F32 scale) {
    c_audio__doppler_scale = scale;
    if (c_audio__doppler_enabled) { FC(g_audio.fmod_system->set3DSettings(scale, 1.0F, c_audio__rolloff_scale), "Could not update doppler scale"); }
}

void audio_draw_3d_dbg() {
    if (!c_debug__audio_info) { return; }

    // Draw listener position and orientation
    Camera3D const camera      = c3d_get();
    Vector3 const listener_pos = camera.position;
    Vector3 const forward      = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 const right        = Vector3CrossProduct(forward, camera.up);

    // Draw listener gizmo
    d3d_line(listener_pos, Vector3Add(listener_pos, Vector3Scale(forward, 3.0F)), CYAN);     // Forward (blue)
    d3d_line(listener_pos, Vector3Add(listener_pos, Vector3Scale(camera.up, 2.0F)), GREEN);  // Up (green)
    d3d_line(listener_pos, Vector3Add(listener_pos, Vector3Scale(right, 2.0F)), RED);        // Right (red)
    d3d_sphere(listener_pos, 0.3F, YELLOW);

    // Draw active 3D sounds
    for (auto &active_sound : g_audio.active_sounds) {
        ActiveSound *sound = &active_sound;
        if (!sound->in_use || !sound->is_3d || !sound->channel) { continue; }

        // Get 3D attributes from FMOD
        FMOD_VECTOR fmod_pos;
        FMOD_VECTOR fmod_vel;
        FMOD_RESULT const result = sound->channel->get3DAttributes(&fmod_pos, &fmod_vel);
        if (result != FMOD_OK) { continue; }

        // Convert back from FMOD coordinate system
        Vector3 const sound_pos = {fmod_pos.x, fmod_pos.y, -fmod_pos.z};
        F32 const distance      = Vector3Distance(listener_pos, sound_pos);

        // Color code by distance and volume
        auto sound_color = WHITE;
        if (distance > c_audio__max_distance) {
            sound_color = GRAY;  // Too far, silent
        } else if (distance < c_audio__min_distance) {
            sound_color = RED;  // Very close, full volume
        } else {
            sound_color = ORANGE;  // In range, distance attenuated
        }

        // Draw sound position
        d3d_sphere(sound_pos, 0.5F, sound_color);

        // Draw line from listener to sound
        Color const line_color = Fade(sound_color, 0.3F);
        d3d_line(listener_pos, sound_pos, line_color);

        // Draw min distance sphere (green wireframe)
        d3d_sphere_wires(sound_pos, c_audio__min_distance, 8, 16, color_sync_blinking_regular(GREEN, DARKGREEN));
        // Draw max distance sphere (red wireframe)
        d3d_sphere(sound_pos, c_audio__max_distance, Fade(RED, 0.33F));
    }
}
