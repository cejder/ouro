#pragma once

#include "common.hpp"
#include "array.hpp"
#include "string.hpp"

#include <raylib.h>

enum IMouse : U8 {
    I_MOUSE_NULL,  // Since we have null here, we need to -1 when checking (raylib does not)
    I_MOUSE_LEFT,
    I_MOUSE_RIGHT,
    I_MOUSE_MIDDLE,
    I_MOUSE_SIDE,
    I_MOUSE_EXTRA,
    I_MOUSE_FORWARD,
    I_MOUSE_BACK,
    I_MOUSE_COUNT,
};

fwd_decl(MouseTape);

#define GAMEPAD_AXIS_DEADZONE 0.1F

fwd_decl(AFont);
fwd_decl(Render);

enum IAction : U8 {
    IA_YES,
    IA_NO,
    IA_OPEN_OVERLAY_MENU,
    IA_NEXT,
    IA_PREVIOUS,
    IA_DECREASE,
    IA_INCREASE,
    IA_TOGGLE_OVERWORLD_AND_DUNGEON_SCENE,
    IA_TURN_3D_LEFT,
    IA_TURN_3D_RIGHT,
    IA_MOVE_3D_FORWARD,
    IA_MOVE_3D_BACKWARD,
    IA_MOVE_3D_LEFT,
    IA_MOVE_3D_RIGHT,
    IA_MOVE_3D_UP,
    IA_MOVE_3D_DOWN,
    IA_MOVE_3D_JUMP,
    IA_MOVE_2D_LEFT,
    IA_MOVE_2D_RIGHT,
    IA_MOVE_2D_UP,
    IA_MOVE_2D_DOWN,
    IA_SCROLL_UP,
    IA_SCROLL_DOWN,
    IA_EDITOR_MOVE_UP,
    IA_EDITOR_MOVE_DOWN,
    IA_EDITOR_MOVE_LEFT,
    IA_EDITOR_MOVE_RIGHT,
    IA_EDITOR_ZOOM_IN,
    IA_EDITOR_ZOOM_OUT,
    IA_EDITOR_BIG_PREVIEW_TOGGLE,
    IA_EDITOR_SKYBOX_TOGGLE,
    IA_EDITOR_DELETE,
    IA_CONSOLE_TOGGLE,
    IA_CONSOLE_SCROLL_UP,
    IA_CONSOLE_SCROLL_DOWN,
    IA_CONSOLE_BACKSPACE,
    IA_CONSOLE_DELETE,
    IA_CONSOLE_HISTORY_PREVIOUS,
    IA_CONSOLE_HISTORY_NEXT,
    IA_CONSOLE_CURSOR_LEFT,
    IA_CONSOLE_CURSOR_RIGHT,
    IA_CONSOLE_AUTOCOMPLETE,
    IA_CONSOLE_EXECUTE,
    IA_CONSOLE_INCREASE_FONT_SIZE,
    IA_CONSOLE_DECREASE_FONT_SIZE,
    IA_CONSOLE_BEGIN_LINE,
    IA_CONSOLE_END_LINE,
    IA_CONSOLE_KILL_LINE,
    IA_CONSOLE_COPY,
    IA_CONSOLE_PASTE,
    IA_DBG_RESET_WINDOWS,
    IA_DBG_TOGGLE_PAUSE_TIME,
    IA_DBG_TOGGLE_CONSOLE_FULLSCREEN,
    IA_DBG_INCREASE_CAMERA_FOVY,
    IA_DBG_DECREASE_CAMERA_FOVY,
    IA_DBG_LOOK_CAMERA_UP,
    IA_DBG_LOOK_CAMERA_DOWN,
    IA_DBG_LOOK_CAMERA_LEFT,
    IA_DBG_LOOK_CAMERA_RIGHT,
    IA_DBG_ROLL_CAMERA_LEFT,
    IA_DBG_ROLL_CAMERA_RIGHT,
    IA_DBG_RESET_CAMERA,
    IA_DBG_LIGHT_1,
    IA_DBG_LIGHT_2,
    IA_DBG_LIGHT_3,
    IA_DBG_LIGHT_4,
    IA_DBG_LIGHT_5,
    IA_DBG_TOGGLE_LIGHT_TYPE_1,
    IA_DBG_TOGGLE_LIGHT_TYPE_2,
    IA_DBG_TOGGLE_LIGHT_TYPE_3,
    IA_DBG_TOGGLE_LIGHT_TYPE_4,
    IA_DBG_TOGGLE_LIGHT_TYPE_5,
    IA_DBG_MOVE_LIGHT_1,
    IA_DBG_MOVE_LIGHT_2,
    IA_DBG_MOVE_LIGHT_3,
    IA_DBG_MOVE_LIGHT_4,
    IA_DBG_MOVE_LIGHT_5,
    IA_DBG_TOGGLE_DBG,
    IA_DBG_TOGGLE_NOCLIP,
    IA_DBG_SELECT_ENTITY,
    IA_DBG_DELETE_ENTITY,
    IA_DBG_ADD_PHYSICS_ENTITY,
    IA_DBG_TURN_ENTITY_LEFT,
    IA_DBG_TURN_ENTITY_RIGHT,
    IA_DBG_MOVE_ENTITY_UP,
    IA_DBG_MOVE_ENTITY_DOWN,
    IA_DBG_MOVE_ENTITY_LEFT,
    IA_DBG_MOVE_ENTITY_RIGHT,
    IA_DBG_MOVE_ENTITY_FORWARD,
    IA_DBG_MOVE_ENTITY_BACKWARD,
    IA_DBG_ROTATE_ENTITY_POS,
    IA_DBG_ROTATE_ENTITY_NEG,
    IA_DBG_MOVE_ENTITY_TO_GROUND,
    IA_DBG_TOGGLE_CAMERA,
    IA_DBG_PULL_DEFAULT_CAM_TO_PLAYER_CAM,
    IA_DBG_WORLD_STATE_RECORD,
    IA_DBG_WORLD_STATE_PLAY,
    IA_DBG_WORLD_STATE_BACKWARD,
    IA_DBG_WORLD_STATE_FORWARD,
    IA_DBG_OPEN_TEST_MENU,
    IA_DBG_CYCLE_VIDEO_RESOLUTION,
    IA_PROFILER_FLAME_GRAPH_SHOW_TOGGLE,
    IA_PROFILER_FLAME_GRAPH_PAUSE_TOGGLE,
    IA_COUNT,
};

enum IModifier : U8 {
    I_MODIFIER_NULL,
    I_MODIFIER_NONE,
    I_MODIFIER_SHIFT,
    I_MODIFIER_ALT,
    I_MODIFIER_CTRL,
    I_MODIFIER_COUNT,
};

struct Keybinding {
    IModifier modifier;
    KeyboardKey primary;
    KeyboardKey secondary;
    IMouse mouse;
    GamepadButton gamepad;
};

struct Input {
    Keybinding keybindings[IA_COUNT];
    S32 main_gamepad_idx;

    BOOL mouse_look_active;

    BOOL action_state[IA_COUNT];
    BOOL previous_action_state[IA_COUNT];

    MouseTape *current_mouse_tape;
    BOOL is_playing_mouse_tape;
    SZ mouse_tape_pos;
};

void input_init();
void input_update();
void input_draw();

BOOL input_is_mouse_look_active();
BOOL input_is_action_down(IAction action);
BOOL input_was_action_down(IAction action);
BOOL input_is_action_up(IAction action);
BOOL input_is_action_pressed(IAction action);
BOOL input_is_action_repeat(IAction action);
BOOL input_is_action_pressed_or_repeat(IAction action);
BOOL input_is_action_released(IAction action);
BOOL input_is_modifier_down(IModifier modifier);
BOOL input_is_mouse_down(IMouse button);
BOOL input_is_mouse_up(IMouse button);
BOOL input_is_mouse_pressed(IMouse button);
BOOL input_is_mouse_released(IMouse button);
Vector2 input_get_mouse_position();
Vector2 input_get_mouse_position_screen();
Vector2 input_get_mouse_delta();
F32 input_get_mouse_wheel();
void input_camera_input_update(F32 dtu, Camera *camera, F32 move_speed);
void input_lighting_input_update();
S32 input_get_main_gamepad_idx();
void input_play_mouse_tape(MouseTape *tape);
void input_pause_mouse_tape();
void input_stop_mouse_tape();

#define is_down(action)              input_is_action_down(action)
#define was_down(action)             input_was_action_down(action)
#define is_up(action)                input_is_action_up(action)
#define is_pressed(action)           input_is_action_pressed(action)
#define is_repeat(action)            input_is_action_repeat(action)
#define is_pressed_or_repeat(action) input_is_action_pressed_or_repeat(action)
#define is_released(action)          input_is_action_released(action)
#define is_mod(modifier)             input_is_modifier_down(modifier)
#define is_mouse_down(button)        input_is_mouse_down(button)
#define is_mouse_up(button)          input_is_mouse_up(button)
#define is_mouse_pressed(button)     input_is_mouse_pressed(button)
#define is_mouse_released(button)    input_is_mouse_released(button)

#define MOUSE_TAPE_NAME_MAX 128
#define MOUSE_TAPES_PATH "tapes/"

struct MouseFrameInfo {
    Vector2 position;
    F32 wheel;
    BOOL mouse_down     [I_MOUSE_COUNT];
    BOOL mouse_up       [I_MOUSE_COUNT];
    BOOL mouse_pressed  [I_MOUSE_COUNT];
    BOOL mouse_released [I_MOUSE_COUNT];
};

ARRAY_DECLARE(MouseFrameInfoArray, MouseFrameInfo);

struct MouseTape {
    MouseFrameInfoArray frames;
    String *name;
};

void mouse_recorder_init(MouseTape *tape, C8 const *name);
void mouse_recorder_record_frame(MouseTape* tape);
void mouse_recorder_reset(MouseTape* tape);
void mouse_recorder_save(MouseTape *tape);
void mouse_recorder_load(MouseTape *tape, C8 const *name);
