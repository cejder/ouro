#include "assert.hpp"
#include "cvar.hpp"
#include "menu.hpp"
#include "audio.hpp"
#include "option.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "string.hpp"

enum MenuChoice : U8 {
    OPTION_CHOICE_BACK_TO_MENU,
    OPTION_CHOICE_SHOW_FRAME_INFO,
    OPTION_CHOICE_RESOLUTION,
    OPTION_CHOICE_VSYNC_ENABLED,
    OPTION_CHOICE_MAX_FPS,
    OPTION_CHOICE_MUSIC_ENABLED,
    OPTION_CHOICE_MUSIC_VOLUME,
    OPTION_CHOICE_SFX_ENABLED,
    OPTION_CHOICE_SFX_VOLUME,
    OPTION_CHOICE_AMBIENCE_ENABLED,
    OPTION_CHOICE_AMBIENCE_VOLUME,
    OPTION_CHOICE_VOICE_ENABLED,
    OPTION_CHOICE_VOICE_VOLUME,
    OPTION_CHOICE_COUNT,
};

SZ menu_extra_option_func_get_entry_count() {
    return OPTION_CHOICE_COUNT;
}

C8 static const *i_labels[OPTION_CHOICE_COUNT] = {
    "Back",
    "Frame Info",
    "Resolution",
    "VSync",
    "FPS",
    "Music",
    "Music Volume",
    "SFX",
    "SFX Volume",
    "Ambience",
    "Ambience Volume",
    "Voice",
    "Voice Volume",
};

C8 const *menu_extra_option_func_get_label(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr) {  // NOLINT
    unused(cb_data);

    _assert_(color_ptr != nullptr, "The color pointer must not be null.");
    _assert_(color_override_ptr != nullptr, "The color override pointer must not be null.");

    if ((MenuChoice)idx == OPTION_CHOICE_BACK_TO_MENU) { menu_extra_override_static_color(color_ptr, color_override_ptr, CREAM); }

    return i_labels[idx];
}

C8 const *menu_extra_option_func_get_desc(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr) {  // NOLINT
    unused(cb_data);
    unused(color_ptr);
    unused(color_override_ptr);

    return "";
}

C8 const *menu_extra_option_func_get_value(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr) {
    unused(cb_data);

    _assert_(color_ptr != nullptr, "The color pointer must not be null.");
    _assert_(color_override_ptr != nullptr, "The color override pointer must not be null.");

    auto option = (MenuChoice)idx;

    switch (option) {
        case OPTION_CHOICE_BACK_TO_MENU: {
            return nullptr;
        };
        case OPTION_CHOICE_SHOW_FRAME_INFO: {
            BOOL const show_frame_info = c_video__fps_info;
            menu_extra_override_toggle_color(color_ptr, color_override_ptr, show_frame_info);
            return TS("%s", show_frame_info ? "On" : "Off")->c;
        };
        case OPTION_CHOICE_RESOLUTION: {
            Vector2 const current_res = {(F32)c_video__render_resolution_width, (F32)c_video__render_resolution_height};
            return TS("%dx%d", (S32)current_res.x, (S32)current_res.y)->c;
        };
        case OPTION_CHOICE_MAX_FPS: {
            return TS("%d", c_video__fps_max)->c;
        };
        case OPTION_CHOICE_VSYNC_ENABLED: {
            BOOL const vsync = c_video__vsync;
            menu_extra_override_toggle_color(color_ptr, color_override_ptr, vsync);
            return TS("%s", vsync ? "On" : "Off")->c;
        };
        case OPTION_CHOICE_MUSIC_ENABLED: {
            BOOL const music_enabled = option_get_audio_enabled(ACG_MUSIC);
            menu_extra_override_toggle_color(color_ptr, color_override_ptr, music_enabled);
            return TS("%s", music_enabled ? "On" : "Off")->c;
        };
        case OPTION_CHOICE_MUSIC_VOLUME: {
            F32 *volume = audio_get_volume_ptr(ACG_MUSIC);
            menu_extra_override_lerp_color(color_ptr, color_override_ptr, OFFCOLOR, ONCOLOR, *volume);
            return TS("%.0f", *volume * 10.0F)->c;
        };
        case OPTION_CHOICE_SFX_ENABLED: {
            BOOL const sfx_enabled = option_get_audio_enabled(ACG_SFX);
            menu_extra_override_toggle_color(color_ptr, color_override_ptr, sfx_enabled);
            return TS("%s", sfx_enabled ? "On" : "Off")->c;
        };
        case OPTION_CHOICE_SFX_VOLUME: {
            F32 *volume = audio_get_volume_ptr(ACG_SFX);
            menu_extra_override_lerp_color(color_ptr, color_override_ptr, OFFCOLOR, ONCOLOR, *volume);
            return TS("%.0f", *volume * 10.0F)->c;
        };
        case OPTION_CHOICE_AMBIENCE_ENABLED: {
            BOOL const ambience_enabled = option_get_audio_enabled(ACG_AMBIENCE);
            menu_extra_override_toggle_color(color_ptr, color_override_ptr, ambience_enabled);
            return TS("%s", ambience_enabled ? "On" : "Off")->c;
        };
        case OPTION_CHOICE_AMBIENCE_VOLUME: {
            F32 *volume = audio_get_volume_ptr(ACG_AMBIENCE);
            menu_extra_override_lerp_color(color_ptr, color_override_ptr, OFFCOLOR, ONCOLOR, *volume);
            return TS("%.0f", *volume * 10.0F)->c;
        };
        case OPTION_CHOICE_VOICE_ENABLED: {
            BOOL const voice_enabled = option_get_audio_enabled(ACG_VOICE);
            menu_extra_override_toggle_color(color_ptr, color_override_ptr, voice_enabled);
            return TS("%s", voice_enabled ? "On" : "Off")->c;
        };
        case OPTION_CHOICE_VOICE_VOLUME: {
            F32 *volume = audio_get_volume_ptr(ACG_VOICE);
            menu_extra_override_lerp_color(color_ptr, color_override_ptr, OFFCOLOR, ONCOLOR, *volume);
            return TS("%.0f", *volume * 10.0F)->c;
        };
        default: {
            _unreachable_();
            return nullptr;
        }
    }
}

void static i_handle_audio(AudioChannelGroup channel) {
    F32 const volume = audio_get_volume(channel);
    audio_set_volume(channel, volume >= 1.0F ? 0.0F : volume + 0.1F);
}

void menu_extra_option_func_yes(void *_cb_data) {
    auto selected_entry = *(SZ *)_cb_data;
    switch (selected_entry) {
        case OPTION_CHOICE_BACK_TO_MENU: {
            switch (g_scenes.current_scene_type) {
            case SCENE_OVERWORLD:
            case SCENE_DUNGEON:{
                    scenes_change_overlay_scene(SCENE_OVERLAY_MENU);
                } break;
                default: {
                    scenes_pop_overlay_scene(true, false);
                } break;
            }
        } break;
        case OPTION_CHOICE_SHOW_FRAME_INFO: {
            option_set_game_show_frame_info(!c_video__fps_info);
        } break;
        case OPTION_CHOICE_RESOLUTION: {
            option_set_video_resolution(option_get_next_video_resolution());
        } break;
        case OPTION_CHOICE_VSYNC_ENABLED: {
            option_set_video_vsync(!c_video__vsync);
        } break;
        case OPTION_CHOICE_MAX_FPS: {
            option_set_video_max_fps(c_video__fps_max == 0 ? GetMonitorRefreshRate(GetCurrentMonitor()) : 0);
        } break;
        case OPTION_CHOICE_MUSIC_ENABLED: {
            option_set_audio_enabled(ACG_MUSIC, !option_get_audio_enabled(ACG_MUSIC));
        } break;
        case OPTION_CHOICE_MUSIC_VOLUME: {
            i_handle_audio(ACG_MUSIC);
        } break;
        case OPTION_CHOICE_SFX_ENABLED: {
            option_set_audio_enabled(ACG_SFX, !option_get_audio_enabled(ACG_SFX));
        } break;
        case OPTION_CHOICE_SFX_VOLUME: {
            i_handle_audio(ACG_SFX);
        } break;
        case OPTION_CHOICE_AMBIENCE_ENABLED: {
            option_set_audio_enabled(ACG_AMBIENCE, !option_get_audio_enabled(ACG_AMBIENCE));
        } break;
        case OPTION_CHOICE_AMBIENCE_VOLUME: {
            i_handle_audio(ACG_AMBIENCE);
        } break;
        case OPTION_CHOICE_VOICE_ENABLED: {
            option_set_audio_enabled(ACG_VOICE, !option_get_audio_enabled(ACG_VOICE));
        } break;
        case OPTION_CHOICE_VOICE_VOLUME: {
            i_handle_audio(ACG_VOICE);
        } break;
        default: {
            _unreachable_();
        }
    }
}

void menu_extra_option_func_change(void *_cb_data, BOOL increase) {
    auto selected_entry = *(SZ *)_cb_data;

    switch (selected_entry) {
        case OPTION_CHOICE_BACK_TO_MENU: {
            // Do nothing.
        } break;
        case OPTION_CHOICE_SHOW_FRAME_INFO: {
            menu_extra_option_func_yes(_cb_data);
        } break;
        case OPTION_CHOICE_RESOLUTION: {
            if (increase) {
                option_set_video_resolution(option_get_next_video_resolution());
            } else {
                option_set_video_resolution(option_get_previous_video_resolution());
            }
        } break;
        case OPTION_CHOICE_VSYNC_ENABLED: {
            menu_extra_option_func_yes(_cb_data);
        } break;
        case OPTION_CHOICE_MAX_FPS: {
            if (increase) {
                option_set_video_max_fps(option_get_next_video_max_fps());
            } else {
                option_set_video_max_fps(option_get_previous_video_max_fps());
            }
        } break;
        case OPTION_CHOICE_MUSIC_ENABLED: {
            menu_extra_option_func_yes(_cb_data);
        } break;
        case OPTION_CHOICE_MUSIC_VOLUME: {
            if (increase) {
                audio_add_volume(ACG_MUSIC, 0.1F);
            } else {
                audio_add_volume(ACG_MUSIC, -0.1F);
            }
        } break;
        case OPTION_CHOICE_SFX_ENABLED: {
            menu_extra_option_func_yes(_cb_data);
        } break;
        case OPTION_CHOICE_SFX_VOLUME: {
            if (increase) {
                audio_add_volume(ACG_SFX, 0.1F);
            } else {
                audio_add_volume(ACG_SFX, -0.1F);
            }
        } break;
        case OPTION_CHOICE_AMBIENCE_ENABLED: {
            menu_extra_option_func_yes(_cb_data);
        } break;
        case OPTION_CHOICE_AMBIENCE_VOLUME: {
            if (increase) {
                audio_add_volume(ACG_AMBIENCE, 0.1F);
            } else {
                audio_add_volume(ACG_AMBIENCE, -0.1F);
            }
        } break;
        case OPTION_CHOICE_VOICE_ENABLED: {
            menu_extra_option_func_yes(_cb_data);
        } break;
        case OPTION_CHOICE_VOICE_VOLUME: {
            if (increase) {
                audio_add_volume(ACG_VOICE, 0.1F);
            } else {
                audio_add_volume(ACG_VOICE, -0.1F);
            }
        } break;
        default: {
            _unreachable_();
        }
    }
}
