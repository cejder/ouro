#include "input.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "color.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "ease.hpp"
#include "light.hpp"
#include "log.hpp"
#include "math.hpp"
#include "message.hpp"
#include "player.hpp"
#include "profiler.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "string.hpp"
#include "time.hpp"

#include <config.h>
#include <raylib.h>
#include <raymath.h>

Input static i_input = {};

C8 static const *i_key_to_cstr(KeyboardKey key) { switch (key) {
case KEY_NULL:                              return "None";
case KEY_A:                                 return "A";
case KEY_B:                                 return "B";
case KEY_C:                                 return "C";
case KEY_D:                                 return "D";
case KEY_E:                                 return "E";
case KEY_F:                                 return "F";
case KEY_G:                                 return "G";
case KEY_H:                                 return "H";
case KEY_I:                                 return "I";
case KEY_J:                                 return "J";
case KEY_K:                                 return "K";
case KEY_L:                                 return "L";
case KEY_M:                                 return "M";
case KEY_N:                                 return "N";
case KEY_O:                                 return "O";
case KEY_P:                                 return "P";
case KEY_Q:                                 return "Q";
case KEY_R:                                 return "R";
case KEY_S:                                 return "S";
case KEY_T:                                 return "T";
case KEY_U:                                 return "U";
case KEY_V:                                 return "V";
case KEY_W:                                 return "W";
case KEY_X:                                 return "X";
case KEY_Y:                                 return "Y";
case KEY_Z:                                 return "Z";
case KEY_ZERO:                              return "0";
case KEY_ONE:                               return "1";
case KEY_TWO:                               return "2";
case KEY_THREE:                             return "3";
case KEY_FOUR:                              return "4";
case KEY_FIVE:                              return "5";
case KEY_SIX:                               return "6";
case KEY_SEVEN:                             return "7";
case KEY_EIGHT:                             return "8";
case KEY_NINE:                              return "9";
case KEY_SPACE:                             return "Space";
case KEY_ESCAPE:                            return "Esc";
case KEY_ENTER:                             return "Enter";
case KEY_TAB:                               return "Tab";
case KEY_BACKSPACE:                         return "Backspace";
case KEY_INSERT:                            return "Insert";
case KEY_DELETE:                            return "Delete";
case KEY_RIGHT:                             return "Right";
case KEY_LEFT:                              return "Left";
case KEY_DOWN:                              return "Down";
case KEY_UP:                                return "Up";
case KEY_PAGE_UP:                           return "Page Up";
case KEY_PAGE_DOWN:                         return "Page Down";
case KEY_HOME:                              return "Home";
case KEY_END:                               return "End";
case KEY_CAPS_LOCK:                         return "Caps Lock";
case KEY_SCROLL_LOCK:                       return "Scroll Lock";
case KEY_NUM_LOCK:                          return "Num Lock";
case KEY_PRINT_SCREEN:                      return "Print Screen";
case KEY_PAUSE:                             return "Pause";
case KEY_F1:                                return "F1";
case KEY_F2:                                return "F2";
case KEY_F3:                                return "F3";
case KEY_F4:                                return "F4";
case KEY_F5:                                return "F5";
case KEY_F6:                                return "F6";
case KEY_F7:                                return "F7";
case KEY_F8:                                return "F8";
case KEY_F9:                                return "F9";
case KEY_F10:                               return "F10";
case KEY_F11:                               return "F11";
case KEY_F12:                               return "F12";
case KEY_LEFT_SHIFT:                        return "Left Shift";
case KEY_LEFT_CONTROL:                      return "Left Ctrl";
case KEY_LEFT_ALT:                          return "Left Alt";
case KEY_LEFT_SUPER:                        return "Left Super";
case KEY_RIGHT_SHIFT:                       return "Right Shift";
case KEY_RIGHT_CONTROL:                     return "Right Ctrl";
case KEY_RIGHT_ALT:                         return "Right Alt";
case KEY_RIGHT_SUPER:                       return "Right Super";
case KEY_KB_MENU:                           return "KB Menu";
case KEY_APOSTROPHE:                        return "'";
case KEY_COMMA:                             return ",";
case KEY_MINUS:                             return "-";
case KEY_PERIOD:                            return ".";
case KEY_SLASH:                             return "/";
case KEY_SEMICOLON:                         return ";";
case KEY_EQUAL:                             return "=";
case KEY_LEFT_BRACKET:                      return "[";
case KEY_BACKSLASH:                         return "\\";
case KEY_RIGHT_BRACKET:                     return "]";
case KEY_GRAVE:                             return "`";
case KEY_KP_0:                              return "Keypad 0";
case KEY_KP_1:                              return "Keypad 1";
case KEY_KP_2:                              return "Keypad 2";
case KEY_KP_3:                              return "Keypad 3";
case KEY_KP_4:                              return "Keypad 4";
case KEY_KP_5:                              return "Keypad 5";
case KEY_KP_6:                              return "Keypad 6";
case KEY_KP_7:                              return "Keypad 7";
case KEY_KP_8:                              return "Keypad 8";
case KEY_KP_9:                              return "Keypad 9";
case KEY_KP_DECIMAL:                        return "Keypad .";
case KEY_KP_DIVIDE:                         return "Keypad /";
case KEY_KP_MULTIPLY:                       return "Keypad *";
case KEY_KP_SUBTRACT:                       return "Keypad -";
case KEY_KP_ADD:                            return "Keypad +";
case KEY_KP_ENTER:                          return "Keypad Enter";
case KEY_KP_EQUAL:                          return "Keypad =";
case KEY_BACK:                              return "Android Back";
case KEY_MENU:                              return "Android Menu";
case KEY_VOLUME_UP:                         return "Volume Up";
case KEY_VOLUME_DOWN:                       return "Volume Down";
default:                                    _unreachable_(); return nullptr; }}
C8 static const *i_modifier_to_cstr(IModifier modifier) { switch (modifier) {
case I_MODIFIER_NULL:                       return "Null";
case I_MODIFIER_NONE:                       return "";
case I_MODIFIER_SHIFT:                      return "Shift+";
case I_MODIFIER_ALT:                        return "Alt+";
case I_MODIFIER_CTRL:                       return "Ctrl+";
default:                                    _unreachable_(); return nullptr; }}
C8 static const *i_mouse_button_to_cstr(IMouse button) { switch (button) {
case I_MOUSE_NULL:                          return "Null";
case I_MOUSE_LEFT:                          return "Left Mouse";
case I_MOUSE_RIGHT:                         return "Right Mouse";
case I_MOUSE_MIDDLE:                        return "Middle Mouse";
case I_MOUSE_SIDE:                          return "Side Mouse";
case I_MOUSE_EXTRA:                         return "Extra Mouse";
case I_MOUSE_FORWARD:                       return "Forward Mouse";
case I_MOUSE_BACK:                          return "Back Mouse";
default:                                    _unreachable_(); return nullptr; }}
C8 static const *i_wheel_to_cstr(IWheel wheel) { switch (wheel) {
case I_WHEEL_NULL:                          return "Null";
case I_WHEEL_UP:                            return "Wheel Up";
case I_WHEEL_DOWN:                          return "Wheel Down";
default:                                    _unreachable_(); return nullptr; }}
C8 static const *i_gamepad_button_to_cstr(GamepadButton button) { switch (button) {
case GAMEPAD_BUTTON_LEFT_FACE_UP:           return "D-Pad Up";
case GAMEPAD_BUTTON_LEFT_FACE_RIGHT:        return "D-Pad Right";
case GAMEPAD_BUTTON_LEFT_FACE_DOWN:         return "D-Pad Down";
case GAMEPAD_BUTTON_LEFT_FACE_LEFT:         return "D-Pad Left";
case GAMEPAD_BUTTON_RIGHT_FACE_UP:          return "Y/Triangle";
case GAMEPAD_BUTTON_RIGHT_FACE_RIGHT:       return "B/Circle";
case GAMEPAD_BUTTON_RIGHT_FACE_DOWN:        return "A/Cross";
case GAMEPAD_BUTTON_RIGHT_FACE_LEFT:        return "X/Square";
case GAMEPAD_BUTTON_LEFT_TRIGGER_1:         return "LB/L1";
case GAMEPAD_BUTTON_LEFT_TRIGGER_2:         return "LT/L2";
case GAMEPAD_BUTTON_RIGHT_TRIGGER_1:        return "RB/R1";
case GAMEPAD_BUTTON_RIGHT_TRIGGER_2:        return "RT/R2";
case GAMEPAD_BUTTON_MIDDLE_LEFT:            return "Back/Select";
case GAMEPAD_BUTTON_MIDDLE:                 return "Guide";
case GAMEPAD_BUTTON_MIDDLE_RIGHT:           return "Start";
case GAMEPAD_BUTTON_LEFT_THUMB:             return "Left Thumb";
case GAMEPAD_BUTTON_RIGHT_THUMB:            return "Right Thumb";
default:                                    _unreachable_(); return nullptr; }}
C8 static const *i_action_to_cstr(IAction action) { switch (action) {
case IA_YES:                                return "YES";
case IA_NO:                                 return "NO";
case IA_OPEN_OVERLAY_MENU:                  return "OPEN MENU";
case IA_NEXT:                               return "NEXT";
case IA_PREVIOUS:                           return "PREVIOUS";
case IA_DECREASE:                           return "DECREASE";
case IA_INCREASE:                           return "INCREASE";
case IA_TOGGLE_CAMERA_PROJECTION:           return "TOGGLE CAMERA PROJECTION";
case IA_TURN_3D_LEFT:                       return "TURN_LEFT";
case IA_TURN_3D_RIGHT:                      return "TURN_RIGHT";
case IA_MOVE_3D_FORWARD:                    return "MOVE FORWARD";
case IA_MOVE_3D_BACKWARD:                   return "MOVE BACKWARD";
case IA_MOVE_3D_LEFT:                       return "MOVE LEFT";
case IA_MOVE_3D_RIGHT:                      return "MOVE RIGHT";
case IA_MOVE_3D_UP:                         return "MOVE UP";
case IA_MOVE_3D_DOWN:                       return "MOVE DOWN";
case IA_MOVE_3D_JUMP:                       return "JUMP";
case IA_MOVE_2D_LEFT:                       return "MOVE 2D LEFT";
case IA_MOVE_2D_RIGHT:                      return "MOVE 2D RIGHT";
case IA_MOVE_2D_UP:                         return "MOVE 2D UP";
case IA_MOVE_2D_DOWN:                       return "MOVE 2D DOWN";
case IA_SCROLL_UP:                          return "SCROLL UP";
case IA_SCROLL_DOWN:                        return "SCROLL DOWN";
case IA_ZOOM_IN:                            return "ZOOM IN";
case IA_ZOOM_OUT:                           return "ZOOM OUT";
case IA_EDITOR_MOVE_UP:                     return "EDITOR UP";
case IA_EDITOR_MOVE_DOWN:                   return "EDITOR DOWN";
case IA_EDITOR_MOVE_LEFT:                   return "EDITOR LEFT";
case IA_EDITOR_MOVE_RIGHT:                  return "EDITOR RIGHT";
case IA_EDITOR_BIG_PREVIEW_TOGGLE:          return "TOGGLE PREVIEW";
case IA_EDITOR_SKYBOX_TOGGLE:               return "TOGGLE SKYBOX";
case IA_EDITOR_DELETE:                      return "DELETE";
case IA_CONSOLE_TOGGLE:                     return "CONSOLE";
case IA_CONSOLE_SCROLL_UP:                  return "CONSOLE UP";
case IA_CONSOLE_SCROLL_DOWN:                return "CONSOLE DOWN";
case IA_CONSOLE_BACKSPACE:                  return "BACKSPACE";
case IA_CONSOLE_DELETE:                     return "DELETE";
case IA_CONSOLE_HISTORY_PREVIOUS:           return "HISTORY PREV";
case IA_CONSOLE_HISTORY_NEXT:               return "HISTORY NEXT";
case IA_CONSOLE_CURSOR_LEFT:                return "CURSOR LEFT";
case IA_CONSOLE_CURSOR_RIGHT:               return "CURSOR RIGHT";
case IA_CONSOLE_AUTOCOMPLETE:               return "AUTOCOMPLETE";
case IA_CONSOLE_EXECUTE:                    return "EXECUTE";
case IA_CONSOLE_INCREASE_FONT_SIZE:         return "FONT SIZE +";
case IA_CONSOLE_DECREASE_FONT_SIZE:         return "FONT SIZE -";
case IA_CONSOLE_BEGIN_LINE:                 return "BEGIN LINE";
case IA_CONSOLE_END_LINE:                   return "END LINE";
case IA_CONSOLE_KILL_LINE:                  return "KILL LINE";
case IA_CONSOLE_COPY:                       return "COPY";
case IA_CONSOLE_PASTE:                      return "PASTE";
case IA_DBG_RESET_WINDOWS:                  return "RESET WINDOWS";
case IA_DBG_TOGGLE_PAUSE_TIME:              return "PAUSE TIME";
case IA_DBG_TOGGLE_CONSOLE_FULLSCREEN:      return "CONSOLE FULLSCREEN";
case IA_DBG_LOOK_CAMERA_UP:                 return "CAM LOOK UP";
case IA_DBG_LOOK_CAMERA_DOWN:               return "CAM LOOK DOWN";
case IA_DBG_LOOK_CAMERA_LEFT:               return "CAM LOOK LEFT";
case IA_DBG_LOOK_CAMERA_RIGHT:              return "CAM LOOK RIGHT";
case IA_DBG_ROLL_CAMERA_LEFT:               return "CAM ROLL LEFT";
case IA_DBG_ROLL_CAMERA_RIGHT:              return "CAM ROLL RIGHT";
case IA_DBG_RESET_CAMERA:                   return "CAM RESET";
case IA_DBG_LIGHT_1:                        return "LIGHT 1";
case IA_DBG_LIGHT_2:                        return "LIGHT 2";
case IA_DBG_LIGHT_3:                        return "LIGHT 3";
case IA_DBG_LIGHT_4:                        return "LIGHT 4";
case IA_DBG_LIGHT_5:                        return "LIGHT 5";
case IA_DBG_TOGGLE_LIGHT_TYPE_1:            return "LIGHT 1 TYPE";
case IA_DBG_TOGGLE_LIGHT_TYPE_2:            return "LIGHT 2 TYPE";
case IA_DBG_TOGGLE_LIGHT_TYPE_3:            return "LIGHT 3 TYPE";
case IA_DBG_TOGGLE_LIGHT_TYPE_4:            return "LIGHT 4 TYPE";
case IA_DBG_TOGGLE_LIGHT_TYPE_5:            return "LIGHT 5 TYPE";
case IA_DBG_MOVE_LIGHT_1:                   return "MOVE LIGHT 1";
case IA_DBG_MOVE_LIGHT_2:                   return "MOVE LIGHT 2";
case IA_DBG_MOVE_LIGHT_3:                   return "MOVE LIGHT 3";
case IA_DBG_MOVE_LIGHT_4:                   return "MOVE LIGHT 4";
case IA_DBG_MOVE_LIGHT_5:                   return "MOVE LIGHT 5";
case IA_DBG_TOGGLE_DBG:                     return "DBG";
case IA_DBG_TOGGLE_NOCLIP:                  return "NOCLIP";
case IA_DBG_SELECT_ENTITY:                  return "SELECT ENTITY";
case IA_DBG_DELETE_ENTITY:                  return "DELETE ENTITY";
case IA_DBG_ADD_PHYSICS_ENTITY:             return "ADD PHYSICS";
case IA_DBG_TURN_ENTITY_LEFT:               return "ENTITY TURN LEFT";
case IA_DBG_TURN_ENTITY_RIGHT:              return "ENTITY TURN RIGHT";
case IA_DBG_MOVE_ENTITY_UP:                 return "ENTITY MOVE UP";
case IA_DBG_MOVE_ENTITY_DOWN:               return "ENTITY MOVE DOWN";
case IA_DBG_MOVE_ENTITY_LEFT:               return "ENTITY MOVE LEFT";
case IA_DBG_MOVE_ENTITY_RIGHT:              return "ENTITY MOVE RIGHT";
case IA_DBG_MOVE_ENTITY_FORWARD:            return "ENTITY MOVE FORWARD";
case IA_DBG_MOVE_ENTITY_BACKWARD:           return "ENTITY MOVE BACK";
case IA_DBG_ROTATE_ENTITY_POS:              return "ENTITY ROTATE +";
case IA_DBG_ROTATE_ENTITY_NEG:              return "ENTITY ROTATE -";
case IA_DBG_MOVE_ENTITY_TO_GROUND:          return "ENTITY GROUND";
case IA_DBG_TOGGLE_CAMERA:                  return "TOGGLE CAM";
case IA_DBG_PULL_DEFAULT_CAM_TO_PLAYER_CAM: return "CAM TO PLAYER";
case IA_DBG_WORLD_STATE_RECORD:             return "RECORD";
case IA_DBG_WORLD_STATE_PLAY:               return "PLAY";
case IA_DBG_WORLD_STATE_BACKWARD:           return "BACKWARD";
case IA_DBG_WORLD_STATE_FORWARD:            return "FORWARD";
case IA_DBG_OPEN_TEST_MENU:                 return "TEST MENU";
case IA_DBG_CYCLE_VIDEO_RESOLUTION:         return "CYCLE RESOLUTION";
case IA_PROFILER_FLAME_GRAPH_SHOW_TOGGLE:   return "FLAME GRAPH SHOW";
case IA_PROFILER_FLAME_GRAPH_PAUSE_TOGGLE:  return "FLAME GRAPH PAUSE";
default:                                    _unreachable_(); return nullptr; }}

void static set(IAction action, IModifier modifier, KeyboardKey primary, KeyboardKey secondary, IMouse mouse, GamepadButton gamepad, IWheel wheel) {
    i_input.keybindings[action].modifier  = modifier;
    i_input.keybindings[action].primary   = primary;
    i_input.keybindings[action].secondary = secondary;
    i_input.keybindings[action].mouse     = mouse;
    i_input.keybindings[action].gamepad   = gamepad;
    i_input.keybindings[action].wheel     = wheel;
}

#define NO_GAMEPAD_ATTACHED_INDEX (-1)

BOOL static i_find_first_controller() {
    for (S32 i = 0; i < MAX_GAMEPADS; i++) {
        if (IsGamepadAvailable(i)) {
            i_input.main_gamepad_idx = i;
            return true;
        }
    }
    i_input.main_gamepad_idx = NO_GAMEPAD_ATTACHED_INDEX;
    return false;
}

#define _kn_ KEY_NULL
#define _mn_ I_MODIFIER_NULL
#define _mon_ I_MOUSE_NULL
#define _gn_ GAMEPAD_BUTTON_UNKNOWN
#define _wn_ I_WHEEL_NULL
void input_init() {
    for (SZ i = 0; i < IA_COUNT; ++i) {
        i_input.action_state[i]          = false;
        i_input.previous_action_state[i] = false;
    }

    if (i_find_first_controller()) {
        llt("Gamepad found at idx %d and is named \"%s\"", i_input.main_gamepad_idx, GetGamepadName(i_input.main_gamepad_idx));
    } else {
        llt("No gamepad found");
    }

//   ACTION                                             MODIFIER,           PRIMARY           SECONDARY   MOUSE,          GAMEPAD                         WHEEL
set( IA_YES,                                            _mn_,               KEY_ENTER,        _kn_,       _mon_,          GAMEPAD_BUTTON_RIGHT_FACE_DOWN,  _wn_         );
set( IA_NO,                                             _mn_,               KEY_BACKSPACE,    _kn_,       _mon_,          GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, _wn_         );
set( IA_OPEN_OVERLAY_MENU,                              _mn_,               KEY_ESCAPE,       _kn_,       _mon_,          GAMEPAD_BUTTON_MIDDLE_RIGHT,     _wn_         );
set( IA_DECREASE,                                       _mn_,               KEY_LEFT,         _kn_,       _mon_,          GAMEPAD_BUTTON_LEFT_FACE_LEFT,   _wn_         );
set( IA_INCREASE,                                       _mn_,               KEY_RIGHT,        _kn_,       _mon_,          GAMEPAD_BUTTON_LEFT_FACE_RIGHT,  _wn_         );
set( IA_TOGGLE_CAMERA_PROJECTION,                       _mn_,               KEY_TAB,          _kn_,       _mon_,          GAMEPAD_BUTTON_MIDDLE_LEFT,      _wn_         );
set( IA_NEXT,                                           _mn_,               KEY_DOWN,         _kn_,       _mon_,          GAMEPAD_BUTTON_LEFT_FACE_DOWN,   _wn_         );
set( IA_PREVIOUS,                                       _mn_,               KEY_UP,           _kn_,       _mon_,          GAMEPAD_BUTTON_LEFT_FACE_UP,     _wn_         );
set( IA_TURN_3D_LEFT,                                   _mn_,               KEY_W,            _kn_,       _mon_,          GAMEPAD_BUTTON_LEFT_TRIGGER_1,   _wn_         );
set( IA_TURN_3D_RIGHT,                                  _mn_,               KEY_R,            _kn_,       _mon_,          GAMEPAD_BUTTON_RIGHT_TRIGGER_1,  _wn_         );
set( IA_MOVE_3D_FORWARD,                                _mn_,               KEY_E,            _kn_,       _mon_,          GAMEPAD_BUTTON_LEFT_FACE_UP,     _wn_         );
set( IA_MOVE_3D_BACKWARD,                               _mn_,               KEY_D,            _kn_,       _mon_,          GAMEPAD_BUTTON_LEFT_FACE_DOWN,   _wn_         );
set( IA_MOVE_3D_LEFT,                                   _mn_,               KEY_S,            _kn_,       _mon_,          GAMEPAD_BUTTON_LEFT_FACE_LEFT,   _wn_         );
set( IA_MOVE_3D_RIGHT,                                  _mn_,               KEY_F,            _kn_,       _mon_,          GAMEPAD_BUTTON_LEFT_FACE_RIGHT,  _wn_         );
set( IA_MOVE_3D_UP,                                     _mn_,               KEY_T,            _kn_,       _mon_,          GAMEPAD_BUTTON_LEFT_TRIGGER_2,   _wn_         );
set( IA_MOVE_3D_DOWN,                                   _mn_,               KEY_G,            _kn_,       _mon_,          GAMEPAD_BUTTON_LEFT_TRIGGER_1,   _wn_         );
set( IA_MOVE_3D_JUMP,                                   _mn_,               KEY_SPACE,        _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_MOVE_2D_LEFT,                                   _mn_,               KEY_S,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_MOVE_2D_RIGHT,                                  _mn_,               KEY_F,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_MOVE_2D_UP,                                     _mn_,               KEY_E,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_MOVE_2D_DOWN,                                   _mn_,               KEY_D,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_SCROLL_UP,                                      _mn_,               KEY_PAGE_UP,      _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_SCROLL_DOWN,                                    _mn_,               KEY_PAGE_DOWN,    _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_ZOOM_IN,                                        I_MODIFIER_NONE,    KEY_Y,            _kn_,       _mon_,          _gn_,                            I_WHEEL_UP   );
set( IA_ZOOM_OUT,                                       I_MODIFIER_NONE,    KEY_H,            _kn_,       _mon_,          _gn_,                            I_WHEEL_DOWN );
set( IA_EDITOR_MOVE_UP,                                 _mn_,               KEY_E,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_EDITOR_MOVE_DOWN,                               _mn_,               KEY_D,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_EDITOR_MOVE_LEFT,                               _mn_,               KEY_S,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_EDITOR_MOVE_RIGHT,                              _mn_,               KEY_F,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_EDITOR_BIG_PREVIEW_TOGGLE,                      _mn_,               KEY_V,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_EDITOR_SKYBOX_TOGGLE,                           _mn_,               KEY_P,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_EDITOR_DELETE,                                  _mn_,               KEY_BACKSPACE,    _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_TOGGLE,                                 I_MODIFIER_NONE,    KEY_F3,           _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_SCROLL_UP,                              _mn_,               KEY_PAGE_UP,      _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_SCROLL_DOWN,                            _mn_,               KEY_PAGE_DOWN,    _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_BACKSPACE,                              _mn_,               KEY_BACKSPACE,    _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_DELETE,                                 _mn_,               KEY_DELETE,       _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_HISTORY_PREVIOUS,                       _mn_,               KEY_UP,           _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_HISTORY_NEXT,                           _mn_,               KEY_DOWN,         _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_CURSOR_LEFT,                            _mn_,               KEY_LEFT,         _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_CURSOR_RIGHT,                           _mn_,               KEY_RIGHT,        _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_AUTOCOMPLETE,                           _mn_,               KEY_TAB,          _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_EXECUTE,                                _mn_,               KEY_ENTER,        _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_INCREASE_FONT_SIZE,                     I_MODIFIER_CTRL,    KEY_EQUAL,        _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_DECREASE_FONT_SIZE,                     I_MODIFIER_CTRL,    KEY_MINUS,        _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_BEGIN_LINE,                             I_MODIFIER_CTRL,    KEY_A,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_END_LINE,                               I_MODIFIER_CTRL,    KEY_E,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_KILL_LINE,                              I_MODIFIER_CTRL,    KEY_K,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_COPY,                                   I_MODIFIER_CTRL,    KEY_C,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_CONSOLE_PASTE,                                  I_MODIFIER_CTRL,    KEY_V,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_RESET_WINDOWS,                              I_MODIFIER_SHIFT,   KEY_F4,           _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_TOGGLE_PAUSE_TIME,                          I_MODIFIER_NONE,    KEY_F1,           _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_TOGGLE_CONSOLE_FULLSCREEN,                  I_MODIFIER_SHIFT,   KEY_END,          _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_LOOK_CAMERA_UP,                             _mn_,               KEY_I,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_LOOK_CAMERA_DOWN,                           _mn_,               KEY_K,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_LOOK_CAMERA_LEFT,                           _mn_,               KEY_J,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_LOOK_CAMERA_RIGHT,                          _mn_,               KEY_L,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_ROLL_CAMERA_LEFT,                           I_MODIFIER_SHIFT,   KEY_U,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_ROLL_CAMERA_RIGHT,                          I_MODIFIER_SHIFT,   KEY_O,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_RESET_CAMERA,                               _mn_,               KEY_C,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_LIGHT_1,                                    I_MODIFIER_NONE,    KEY_ONE,          _kn_,       _mon_,          GAMEPAD_BUTTON_RIGHT_THUMB,      _wn_         );
set( IA_DBG_LIGHT_2,                                    I_MODIFIER_NONE,    KEY_TWO,          _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_LIGHT_3,                                    I_MODIFIER_NONE,    KEY_THREE,        _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_LIGHT_4,                                    I_MODIFIER_NONE,    KEY_FOUR,         _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_LIGHT_5,                                    I_MODIFIER_NONE,    KEY_FIVE,         _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_TOGGLE_LIGHT_TYPE_1,                        I_MODIFIER_SHIFT,   KEY_ONE,          _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_TOGGLE_LIGHT_TYPE_2,                        I_MODIFIER_SHIFT,   KEY_TWO,          _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_TOGGLE_LIGHT_TYPE_3,                        I_MODIFIER_SHIFT,   KEY_THREE,        _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_TOGGLE_LIGHT_TYPE_4,                        I_MODIFIER_SHIFT,   KEY_FOUR,         _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_TOGGLE_LIGHT_TYPE_5,                        I_MODIFIER_SHIFT,   KEY_FIVE,         _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_MOVE_LIGHT_1,                               I_MODIFIER_CTRL,    KEY_ONE,          _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_MOVE_LIGHT_2,                               I_MODIFIER_CTRL,    KEY_TWO,          _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_MOVE_LIGHT_3,                               I_MODIFIER_CTRL,    KEY_THREE,        _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_MOVE_LIGHT_4,                               I_MODIFIER_CTRL,    KEY_FOUR,         _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_MOVE_LIGHT_5,                               I_MODIFIER_CTRL,    KEY_FIVE,         _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_TOGGLE_DBG,                                 I_MODIFIER_NONE,    KEY_F4,           _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_TOGGLE_NOCLIP,                              _mn_,               KEY_N,            _kn_,       _mon_,          GAMEPAD_BUTTON_RIGHT_FACE_LEFT,  _wn_         );
set( IA_DBG_DELETE_ENTITY,                              I_MODIFIER_NONE,    KEY_DELETE,       _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_ADD_PHYSICS_ENTITY,                         I_MODIFIER_CTRL,    KEY_DELETE,       _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_SELECT_ENTITY,                              I_MODIFIER_NONE,    KEY_ENTER,        _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_TURN_ENTITY_LEFT,                           I_MODIFIER_SHIFT,   KEY_M,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_TURN_ENTITY_RIGHT,                          I_MODIFIER_SHIFT,   KEY_COMMA,        _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_MOVE_ENTITY_UP,                             _mn_,               KEY_PAGE_UP,      _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_MOVE_ENTITY_DOWN,                           _mn_,               KEY_PAGE_DOWN,    _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_MOVE_ENTITY_LEFT,                           _mn_,               KEY_LEFT,         _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_MOVE_ENTITY_RIGHT,                          _mn_,               KEY_RIGHT,        _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_MOVE_ENTITY_FORWARD,                        _mn_,               KEY_UP,           _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_MOVE_ENTITY_BACKWARD,                       _mn_,               KEY_DOWN,         _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_ROTATE_ENTITY_POS,                          _mn_,               KEY_M,            _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_ROTATE_ENTITY_NEG,                          _mn_,               KEY_COMMA,        _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_MOVE_ENTITY_TO_GROUND,                      I_MODIFIER_ALT,     KEY_ENTER,        _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_TOGGLE_CAMERA,                              I_MODIFIER_NONE,    KEY_F6,           _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_PULL_DEFAULT_CAM_TO_PLAYER_CAM,             I_MODIFIER_SHIFT,   KEY_F6,           _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_WORLD_STATE_RECORD,                         I_MODIFIER_NONE,    KEY_F8,           _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_WORLD_STATE_PLAY,                           I_MODIFIER_SHIFT,   KEY_F8,           _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_WORLD_STATE_BACKWARD,                       I_MODIFIER_NONE,    KEY_F9,           _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_WORLD_STATE_FORWARD,                        I_MODIFIER_NONE,    KEY_F10,          _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_DBG_OPEN_TEST_MENU,                             _mn_,               KEY_SEMICOLON,    _kn_,       _mon_,          GAMEPAD_BUTTON_RIGHT_FACE_UP,    _wn_         );
set( IA_DBG_CYCLE_VIDEO_RESOLUTION,                     I_MODIFIER_SHIFT,   KEY_F1,           _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_PROFILER_FLAME_GRAPH_SHOW_TOGGLE,               I_MODIFIER_NONE,    KEY_F5,           _kn_,       _mon_,          _gn_,                            _wn_         );
set( IA_PROFILER_FLAME_GRAPH_PAUSE_TOGGLE,              I_MODIFIER_SHIFT,   KEY_F5,           _kn_,       _mon_,          _gn_,                            _wn_         );
}
#undef _kn_
#undef _mn_
#undef _mon_
#undef _gn_
#undef _wn_

void input_update() {
    BOOL static first_time          = true;
    BOOL const gamepad_disconnected = i_input.main_gamepad_idx == NO_GAMEPAD_ATTACHED_INDEX || !IsGamepadAvailable(i_input.main_gamepad_idx);

    // Store current states for next frame
    for (SZ i = 0; i < IA_COUNT; ++i) {
        i_input.previous_action_state[i] = i_input.action_state[i];
        i_input.action_state[i]          = input_is_action_down((IAction)i);
    }

    // Handle gamepad disconnection
    if (!first_time && gamepad_disconnected) {
        if (i_input.main_gamepad_idx != NO_GAMEPAD_ATTACHED_INDEX) {
            mqod("Gamepad \\ouc{#ff0000ff}not connected", 6);
            i_input.main_gamepad_idx = NO_GAMEPAD_ATTACHED_INDEX;
        }

        // Try to find a new gamepad
        if (i_find_first_controller()) {
            C8 const *name = GetGamepadName(i_input.main_gamepad_idx);
            mqod(TS("Gamepad \"%s\" \\ouc{#00ff00ff}connected", name)->c, 6);
        }
    }

    first_time = false;

    // Advance mouse tape playback
    if (i_input.is_playing_mouse_tape) {
        i_input.mouse_tape_pos++;
        if (i_input.mouse_tape_pos >= i_input.current_mouse_tape->frames.count) {
            input_stop_mouse_tape();
        }
    }
}

void input_draw() {
    if (!c_debug__keybindings_info) { return; }

    F32 const padding           = 5.0F;
    Vector2 const offset        = {padding, padding};
    Vector2 text_pos            = {padding, padding};
    Vector2 const shadow_offset = {1.0F, 1.0F};
    Color const bg              = {0, 0, 0, 120};
    auto const base_color       = WHITE;
    auto const shadow_color     = BLACK;
    AFont *font                 = asset_get_font("spleen-6x12", 12);

    // Create string arrays for each column
    auto **action_strings  = mmta(String **, sizeof(String *) * IA_COUNT);
    auto **keybind_strings = mmta(String **, sizeof(String *) * IA_COUNT);
    auto **gamepad_strings = mmta(String **, sizeof(String *) * IA_COUNT);
    auto **mouse_strings   = mmta(String **, sizeof(String *) * IA_COUNT);
    auto **wheel_strings   = mmta(String **, sizeof(String *) * IA_COUNT);

    for (SZ i = 0; i < IA_COUNT; ++i) {
        action_strings[i]  = string_create_empty(MEMORY_TYPE_ARENA_TRANSIENT);
        keybind_strings[i] = string_create_empty(MEMORY_TYPE_ARENA_TRANSIENT);
        gamepad_strings[i] = string_create_empty(MEMORY_TYPE_ARENA_TRANSIENT);
        mouse_strings[i]   = string_create_empty(MEMORY_TYPE_ARENA_TRANSIENT);
        wheel_strings[i]   = string_create_empty(MEMORY_TYPE_ARENA_TRANSIENT);
    }

    // First pass: create strings and calculate maximum widths
    F32 max_action_width  = 0;
    F32 max_keybind_width = 0;
    F32 max_gamepad_width = 0;
    F32 max_mouse_width   = 0;
    F32 max_wheel_width   = 0;

    // Color for when actions are actually active in some way.
    C8 const *down_color     = "\\ouc{#00ff00ff}";
    C8 const *released_color = "\\ouc{#ff0000ff}";
    C8 const *pressed_color  = "\\ouc{#00ffffff}";
    C8 const *mod_color      = "\\ouc{#00ffffff}";
    C8 const *key_color      = "\\ouc{#aaffffff}";
    C8 const *or_color       = "\\ouc{#aaaaffff}";
    C8 const *gamepad_color  = "\\ouc{#ffccddff}";
    C8 const *mouse_color    = "\\ouc{#ff22ddff}";
    C8 const *wheel_color    = "\\ouc{#ffaa22ff}";

    for (SZ i = 0; i < IA_COUNT; ++i) {
        Keybinding const *kb = &i_input.keybindings[i];

        // Change action name color based on state
        C8 const *action_color = "\\ouc{#cc88c0ff}";
        if (input_is_action_down((IAction)i)) {
            action_color = down_color;
        } else if (input_is_action_released((IAction)i)) {
            action_color = released_color;
        } else if (input_is_action_pressed((IAction)i)) {
            action_color = pressed_color;
        }

        // Create and measure action string
        string_appendf(action_strings[i], "%s%s", action_color, i_action_to_cstr((IAction)i));
        F32 const action_width = measure_text_ouc(font, action_strings[i]->c).x;
        max_action_width = glm::max(max_action_width, action_width);

        // Create and measure keybind string
        if (kb->modifier != I_MODIFIER_NULL) { string_appendf(keybind_strings[i], "%s%s",       mod_color,           i_modifier_to_cstr(kb->modifier)); }
        if (kb->primary != KEY_NULL)         { string_appendf(keybind_strings[i], "%s%s",       key_color,           i_key_to_cstr(kb->primary));       }
        if (kb->secondary != KEY_NULL)       { string_appendf(keybind_strings[i], "%s or %s%s", or_color, key_color, i_key_to_cstr(kb->secondary));     }
        F32 const keybind_width = measure_text_ouc(font, keybind_strings[i]->c).x;
        max_keybind_width = glm::max(max_keybind_width, keybind_width);

        // Create and measure gamepad string if present
        if (kb->gamepad != GAMEPAD_BUTTON_UNKNOWN) {
            string_appendf(gamepad_strings[i], "%s%s", gamepad_color, i_gamepad_button_to_cstr(kb->gamepad));
            F32 const gamepad_width = measure_text_ouc(font, gamepad_strings[i]->c).x;
            max_gamepad_width       = glm::max(max_gamepad_width, gamepad_width);
        }

        // Create and measure mouse string if present
        if (kb->mouse != I_MOUSE_NULL) {
            string_appendf(mouse_strings[i], "%s%s", mouse_color, i_mouse_button_to_cstr(kb->mouse));
            F32 const mouse_width = measure_text_ouc(font, mouse_strings[i]->c).x;
            max_mouse_width       = glm::max(max_mouse_width, mouse_width);
        }

        // Create and measure wheel string if present
        if (kb->wheel != I_WHEEL_NULL) {
            string_appendf(wheel_strings[i], "%s%s", wheel_color, i_wheel_to_cstr(kb->wheel));
            F32 const wheel_width = measure_text_ouc(font, wheel_strings[i]->c).x;
            max_wheel_width       = glm::max(max_wheel_width, wheel_width);
        }
    }

    // Use initial position as padding reference
    F32 const edge_padding = text_pos.x;  // Use the initial x position as the standard padding
    F32 const col_padding  = 20.0F;        // Padding between columns

    // Define column positions with mouse and wheel columns
    F32 const col1_x     = text_pos.x;
    F32 const col2_x     = col1_x + max_action_width + col_padding;
    F32 const col3_x     = col2_x + max_keybind_width + col_padding;
    F32 const col4_x     = col3_x + max_gamepad_width + col_padding;
    F32 const col5_x     = col4_x + max_mouse_width + col_padding;
    F32 const row_height = (F32)font->font_size;

    // Calculate total dimensions based on content and edge padding
    F32 const total_width  = col5_x + max_wheel_width - text_pos.x + edge_padding;
    F32 const total_height = row_height * (F32)IA_COUNT;

    // Draw background with consistent padding on all sides
    d2d_rectangle((S32)offset.x, (S32)offset.y, (S32)(total_width + (2.0F * edge_padding)), (S32)(total_height + (2.0F * edge_padding)), bg);

    // Second pass: draw using stored strings
    for (SZ i = 0; i < IA_COUNT; ++i) {
        Vector2 const action_pos  = {col1_x + offset.x, text_pos.y + offset.y};
        Vector2 const key_pos     = {col2_x + offset.x, text_pos.y + offset.y};
        Vector2 const gamepad_pos = {col3_x + offset.x, text_pos.y + offset.y};
        Vector2 const mouse_pos   = {col4_x + offset.x, text_pos.y + offset.y};
        Vector2 const wheel_pos   = {col5_x + offset.x, text_pos.y + offset.y};

        // Draw stored strings
        d2d_text_ouc_shadow(font, action_strings[i]->c, action_pos, base_color, shadow_color, shadow_offset);
        d2d_text_ouc_shadow(font, keybind_strings[i]->c, key_pos, base_color, shadow_color, shadow_offset);

        if (i_input.keybindings[i].gamepad != GAMEPAD_BUTTON_UNKNOWN) {
            d2d_text_ouc_shadow(font, gamepad_strings[i]->c, gamepad_pos, base_color, shadow_color, shadow_offset);
        }

        if (i_input.keybindings[i].mouse != I_MOUSE_NULL) {
            d2d_text_ouc_shadow(font, mouse_strings[i]->c, mouse_pos, base_color, shadow_color, shadow_offset);
        }

        if (i_input.keybindings[i].wheel != I_WHEEL_NULL) {
            d2d_text_ouc_shadow(font, wheel_strings[i]->c, wheel_pos, base_color, shadow_color, shadow_offset);
        }

        text_pos.y += row_height;
    }

    // Draw stick grid if gamepad is attached
    if (i_input.main_gamepad_idx == NO_GAMEPAD_ATTACHED_INDEX) { return; }

    // Constants for grid layout
    F32 const grid_size   = 150.0F;
    F32 const grid_half   = grid_size / 2.0F;
    F32 const dot_radius  = grid_size / 20.0F;
    F32 const stick_scale = (grid_size / 2.0F) - dot_radius;

    // Colors
    Color const grid_bg    = bg;
    Color const grid_lines = Fade(NAYBEIGE, 0.5F);
    Color const left_dot   = EVAORANGE;
    Color const right_dot  = EVAGREEN;

    Vector2 const left_stick = {
        GetGamepadAxisMovement(i_input.main_gamepad_idx, GAMEPAD_AXIS_LEFT_X),
        GetGamepadAxisMovement(i_input.main_gamepad_idx, GAMEPAD_AXIS_LEFT_Y),
    };
    Vector2 const right_stick = {
        GetGamepadAxisMovement(i_input.main_gamepad_idx, GAMEPAD_AXIS_RIGHT_X),
        GetGamepadAxisMovement(i_input.main_gamepad_idx, GAMEPAD_AXIS_RIGHT_Y),
    };

    // Left stick grid
    Vector2 grid_pos = {text_pos.x + total_width + (offset.x * 3.0F), offset.y};
    Vector2 center   = {grid_pos.x + grid_half, grid_pos.y + grid_half};
    d2d_rectangle((S32)grid_pos.x, (S32)grid_pos.y, (S32)grid_size, (S32)grid_size, grid_bg);
    d2d_line({center.x, grid_pos.y}, {center.x, grid_pos.y + grid_size}, grid_lines);
    d2d_line({grid_pos.x, center.y}, {grid_pos.x + grid_size, center.y}, grid_lines);
    d2d_circle({center.x + (left_stick.x * stick_scale), center.y + (left_stick.y * stick_scale)}, dot_radius, left_dot);

    // Right stick grid
    grid_pos.x += grid_size + padding;
    center      = {grid_pos.x + grid_half, grid_pos.y + grid_half};
    d2d_rectangle((S32)grid_pos.x, (S32)grid_pos.y, (S32)grid_size, (S32)grid_size, grid_bg);
    d2d_line({center.x, grid_pos.y}, {center.x, grid_pos.y + grid_size}, grid_lines);
    d2d_line({grid_pos.x, center.y}, {grid_pos.x + grid_size, center.y}, grid_lines);
    d2d_circle({center.x + (right_stick.x * stick_scale), center.y + (right_stick.y * stick_scale)}, dot_radius, right_dot);
}

BOOL static i_mod_check(IModifier modifier) {
    // INFO: The returned boolean signifies if the action should be considered.
    //  The mod check has three potential states:
    // 1. The modifier is null, in which case we don't care about the modifier.
    if (modifier == I_MODIFIER_NULL) { return true; }
    // 2. The modifier is none, in which case we expect that no modifier is down.
    if (modifier == I_MODIFIER_NONE) { return !is_mod(I_MODIFIER_SHIFT) && !is_mod(I_MODIFIER_CTRL) && !is_mod(I_MODIFIER_ALT); }
    // 3. The modifier is a specific modifier, in which case we care if that specific modifier is down.
    return is_mod(modifier);
}

#define MOUSE_CHECK(state, action) \
(i_input.keybindings[action].mouse != I_MOUSE_NULL && IsMouseButton##state(i_input.keybindings[action].mouse - 1))

BOOL static i_wheel_check(IAction action) {
    IWheel const wheel = i_input.keybindings[action].wheel;
    if (wheel == I_WHEEL_NULL) { return false; }
    F32 const wheel_move = input_get_mouse_wheel();
    if (wheel == I_WHEEL_UP && wheel_move > 0.0F) { return true; }
    if (wheel == I_WHEEL_DOWN && wheel_move < 0.0F) { return true; }
    return false;
}

#define WHEEL_CHECK(action) i_wheel_check(action)

#define KEY_CHECK(state, action) \
( \
IsKey##state(i_input.keybindings[action].primary) || \
IsKey##state(i_input.keybindings[action].secondary) || \
MOUSE_CHECK(state, action) || \
IsGamepadButton##state(i_input.main_gamepad_idx, i_input.keybindings[action].gamepad) || \
WHEEL_CHECK(action) \
)

#define KEY_CHECK_SLIM(state, action) \
(                                                                   \
IsKey##state(i_input.keybindings[action].primary)|| \
IsKey##state(i_input.keybindings[action].secondary) \
)

#define MOD_CHECK(action) \
i_mod_check(i_input.keybindings[action].modifier)

BOOL input_is_mouse_look_active() {
    return i_input.mouse_look_active;
}

BOOL input_was_action_down(IAction action) {
    return i_input.previous_action_state[action];
}

BOOL input_is_action_down(IAction action) {
    return MOD_CHECK(action) && (KEY_CHECK(Down, action));
}

BOOL input_is_action_up(IAction action) {
    return MOD_CHECK(action) && (KEY_CHECK(Up, action));
}

BOOL input_is_action_pressed(IAction action) {
    return MOD_CHECK(action) && KEY_CHECK(Pressed, action);
}

BOOL input_is_action_repeat(IAction action) {
    return MOD_CHECK(action) && KEY_CHECK_SLIM(PressedRepeat, action);
}

BOOL input_is_action_pressed_or_repeat(IAction action) {
    if (!MOD_CHECK(action)) { return false; }

    // Check for standard keyboard/mouse/gamepad presses
    if (KEY_CHECK(Pressed, action)) { return true; }

    // Check for keyboard repeat (only works for keyboard)
    if (KEY_CHECK_SLIM(PressedRepeat, action)) { return true; }

    // Add custom gamepad button repeat functionality (time-based)
    F32 static gamepad_hold_time[IA_COUNT]         = {};
    BOOL static gamepad_repeat_triggered[IA_COUNT] = {false};

    // Settings for repeat behavior (in seconds)
    F32 const initial_delay   = 0.25F;   // Initial delay before first repeat (~15 frames at 60 FPS)
    F32 const repeat_interval = 0.1F;  // Interval between repeats (~6 frames at 60 FPS)

    F32 const dt = time_get_delta();

    if (i_input.main_gamepad_idx != NO_GAMEPAD_ATTACHED_INDEX && i_input.keybindings[action].gamepad != GAMEPAD_BUTTON_UNKNOWN) {
        BOOL const is_button_down = IsGamepadButtonDown(i_input.main_gamepad_idx, i_input.keybindings[action].gamepad);

        if (is_button_down) {
            gamepad_hold_time[action] += dt;

            // First check if we're past the initial delay
            if (gamepad_hold_time[action] > initial_delay) {
                // Calculate how far we are into the repeat cycle
                F32 const time_since_delay = gamepad_hold_time[action] - initial_delay;

                // Check if we've reached a new repeat interval
                BOOL const should_trigger = (F32)(S32)(time_since_delay / repeat_interval) > (F32)(S32)((time_since_delay - dt) / repeat_interval);

                if (should_trigger && !gamepad_repeat_triggered[action]) {
                    gamepad_repeat_triggered[action] = true;
                    return true;
                }
                if (!should_trigger) { gamepad_repeat_triggered[action] = false; }
            }
        } else {
            // Reset when button is released
            gamepad_hold_time[action]        = 0.0F;
            gamepad_repeat_triggered[action] = false;
        }
    }

    return false;
}

BOOL input_is_action_released(IAction action) {
    return MOD_CHECK(action) && KEY_CHECK(Released, action);
}

BOOL input_is_modifier_down(IModifier modifier) {
    switch (modifier) {
        case I_MODIFIER_SHIFT: {
            return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        }
        case I_MODIFIER_CTRL: {
            return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        }
        case I_MODIFIER_ALT: {
            return IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        }
        default: {
            _unreachable_();
            return false;
        }
    }
}

BOOL input_is_mouse_down(IMouse button) {
    if (i_input.is_playing_mouse_tape) {
        return i_input.current_mouse_tape->frames.data[i_input.mouse_tape_pos].mouse_down[button];
    }
    return IsMouseButtonDown(button - 1);
}

BOOL input_is_mouse_up(IMouse button) {
    if (i_input.is_playing_mouse_tape) {
        return i_input.current_mouse_tape->frames.data[i_input.mouse_tape_pos].mouse_up[button];
    }
    return IsMouseButtonUp(button - 1);
}

BOOL input_is_mouse_pressed(IMouse button) {
    if (i_input.is_playing_mouse_tape) {
        return i_input.current_mouse_tape->frames.data[i_input.mouse_tape_pos].mouse_pressed[button];
    }
    return IsMouseButtonPressed(button - 1);
}

BOOL input_is_mouse_released(IMouse button) {
    if (i_input.is_playing_mouse_tape) {
        return i_input.current_mouse_tape->frames.data[i_input.mouse_tape_pos].mouse_released[button];
    }
    return IsMouseButtonReleased(button - 1);
}

Vector2 input_get_mouse_position() {
    if (i_input.is_playing_mouse_tape) {
        return i_input.current_mouse_tape->frames.data[i_input.mouse_tape_pos].position;
    }
    return Vector2Multiply(GetMousePosition(), Vector2Divide(render_get_render_resolution(), render_get_window_resolution()));
}

Vector2 input_get_mouse_position_screen() {
    return GetMousePosition();
}

Vector2 input_get_mouse_delta() {
    return Vector2Multiply(GetMouseDelta(), Vector2Divide(render_get_render_resolution(), render_get_window_resolution()));
}

F32 input_get_mouse_wheel() {
    if (i_input.is_playing_mouse_tape) {
        return i_input.current_mouse_tape->frames.data[i_input.mouse_tape_pos].wheel;
    }
    return GetMouseWheelMove();
}

// TODO: The way we calculate the deadzone is right now square, but it should be circular. (I think)

void input_camera_input_update(F32 dtu, Camera *camera, F32 move_speed) {
    // We don't want to move the camera etc. if the console is enabled.
    if (c_console__enabled) { return; }

    // General stuff
    Vector3 movement = {};
    Vector3 rotation = {};

    BOOL const shift_down = is_mod(I_MODIFIER_SHIFT) || IsGamepadButtonDown(i_input.main_gamepad_idx, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
    BOOL const ctrl_down  = is_mod(I_MODIFIER_CTRL) || IsGamepadButtonDown(i_input.main_gamepad_idx, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);

    move_speed = move_speed * dtu;
    if (shift_down) {
        move_speed *= 10.0F;
    } else if (ctrl_down) {
        move_speed /= 10.0F;
    }

    F32 keyboard_rotation_speed = 75.0F * dtu;
    if (shift_down) {
        keyboard_rotation_speed *= 2.0F;
    } else if (ctrl_down) {
        keyboard_rotation_speed /= 10.0F;
    }

    F32 fovy_speed = 15.0F * dtu;
    if (shift_down) {
        fovy_speed *= 2.0F;
    } else if (ctrl_down) {
        fovy_speed /= 10.0F;
    }

    // Move camera with keyboard.
    if (is_down(IA_MOVE_3D_LEFT)) {
        movement.y = -1.0F;
    } else if (is_down(IA_MOVE_3D_RIGHT)) {
        movement.y = 1.0F;
    }

    if (is_down(IA_MOVE_3D_FORWARD)) {
        movement.x = 1.0F;
    } else if (is_down(IA_MOVE_3D_BACKWARD)) {
        movement.x = -1.0F;
    }

    if (is_down(IA_MOVE_3D_UP)) {
        movement.z = 1.0F;
    } else if (is_down(IA_MOVE_3D_DOWN)) {
        movement.z = -1.0F;
    }

    if (movement.x != 0.0F || movement.y != 0.0F || movement.z != 0.0F) {
        movement = Vector3Normalize(movement);
        movement = Vector3Scale(movement, move_speed);
    }

    // If we did not move with the keyboard, we check the gamepad but we ignore up and down movement.
    if (movement.x == 0.0F && movement.y == 0.0F) {
        // NOTE: X and Y are swapped because the gamepad is rotated 90 degrees compared to the keyboard. (?)
        F32 const x = -GetGamepadAxisMovement(i_input.main_gamepad_idx, GAMEPAD_AXIS_LEFT_Y);
        F32 const y =  GetGamepadAxisMovement(i_input.main_gamepad_idx, GAMEPAD_AXIS_LEFT_X);
        if (math_abs_f32(x) > GAMEPAD_AXIS_DEADZONE) { movement.x = x * move_speed; }
        if (math_abs_f32(y) > GAMEPAD_AXIS_DEADZONE) { movement.y = y * move_speed; }
    }

    // Rotate camera with keyboard (not possible in ortho)
    if (camera->projection != CAMERA_ORTHOGRAPHIC) {
        if (is_down(IA_DBG_LOOK_CAMERA_LEFT)) {
            rotation.x = -1.0F;
        } else if (is_down(IA_DBG_LOOK_CAMERA_RIGHT)) {
            rotation.x = 1.0F;
        }

        if (is_down(IA_DBG_LOOK_CAMERA_UP)) {
            rotation.y = -1.0F;
        } else if (is_down(IA_DBG_LOOK_CAMERA_DOWN)) {
            rotation.y = 1.0F;
        }

        if (is_down(IA_DBG_ROLL_CAMERA_LEFT)) {
            rotation.z = -1.0F;
        } else if (is_down(IA_DBG_ROLL_CAMERA_RIGHT)) {
            rotation.z = 1.0F;
        }

        if (rotation.x != 0.0F || rotation.y != 0.0F || rotation.z != 0.0F) {
            rotation = Vector3Normalize(rotation);
            rotation = Vector3Scale(rotation, keyboard_rotation_speed);
        }
    }

    // Handle mouse look when side button is held
    BOOL static was_mouse_down = false;
    BOOL const is_side_down    = is_mouse_down(I_MOUSE_SIDE);
    if (is_side_down && !was_mouse_down) {
        i_input.mouse_look_active = true;
        DisableCursor();  // Lock and hide cursor when button is first pressed
    } else if (!is_side_down && was_mouse_down) {
        i_input.mouse_look_active = false;
        EnableCursor();  // Restore cursor when button is released
    }
    was_mouse_down = is_side_down;

    if (is_side_down) {
        Vector2 const mouse_delta      = input_get_mouse_delta();
        F32 const mouse_rotation_speed = 0.05F;
        if (mouse_delta.x != 0.0F) { rotation.x = mouse_delta.x * mouse_rotation_speed; }
        if (mouse_delta.y != 0.0F) { rotation.y = mouse_delta.y * mouse_rotation_speed; }
    }

    // Handle RTS-style pan with middle mouse in orthographic mode
    BOOL static was_middle_down = false;
    BOOL const is_middle_down   = is_mouse_down(I_MOUSE_MIDDLE);

    if (camera->projection == CAMERA_ORTHOGRAPHIC) {
        if (is_middle_down && !was_middle_down) {
            DisableCursor();  // Lock and hide cursor when button is first pressed
        } else if (!is_middle_down && was_middle_down) {
            EnableCursor();  // Restore cursor when button is released
        }

        if (is_middle_down) {
            Vector2 const mouse_delta = input_get_mouse_delta();
            F32 const pan_speed = CAMERA_ORTHO_PAN_SPEED * (camera->fovy / 100.0F);  // Scale with zoom level

            // Calculate camera forward and right vectors
            Vector3 const forward = Vector3Normalize(Vector3Subtract(camera->target, camera->position));
            Vector3 const right = Vector3Normalize(Vector3CrossProduct(forward, camera->up));
            Vector3 const up = Vector3CrossProduct(right, forward);

            // Apply pan movement (inverted for natural feel)
            Vector3 const pan_offset = Vector3Add(
                Vector3Scale(right, -mouse_delta.x * pan_speed),
                Vector3Scale(up, mouse_delta.y * pan_speed)
            );

            camera->position = Vector3Add(camera->position, pan_offset);
            camera->target = Vector3Add(camera->target, pan_offset);
        }
    }
    was_middle_down = is_middle_down;

    // If we did not rotate with the keyboard, we check the gamepad but we ignore roll.
    if (rotation.x == 0.0F && rotation.y == 0.0F) {
        F32 const gamepad_rotation_speed = 180.0F * dtu;
        F32 const x                      = GetGamepadAxisMovement(i_input.main_gamepad_idx, GAMEPAD_AXIS_RIGHT_X);
        F32 const y                      = GetGamepadAxisMovement(i_input.main_gamepad_idx, GAMEPAD_AXIS_RIGHT_Y);
        if (math_abs_f32(x) > GAMEPAD_AXIS_DEADZONE) { rotation.x = x * gamepad_rotation_speed; }
        if (math_abs_f32(y) > GAMEPAD_AXIS_DEADZONE) { rotation.y = y * gamepad_rotation_speed; }
    }

    // Change camera fovy/zoom with smooth easing
    // Configurable zoom parameters (adjust these to tune the feel)
    F32 static const zoom_per_tick    = 5.0F;     // How much to zoom per mouse wheel tick
    F32 static const zoom_ease_speed  = 8.0F;     // Higher = faster easing (recommended: 5-15)

    // State tracking
    F32 static target_fovy = 45.0F;
    F32 static previous_cam_fovy = 45.0F;

    // Detect camera projection toggle by checking if fovy changed externally
    F32 const current_fovy = c3d_get_fov();
    if (glm::abs(current_fovy - previous_cam_fovy) > 5.0F) {
        target_fovy = current_fovy;
    }

    // Handle zoom input - adjust target fovy
    if (is_pressed(IA_ZOOM_OUT)) {
        target_fovy += zoom_per_tick;
    }
    if (is_pressed(IA_ZOOM_IN)) {
        target_fovy -= zoom_per_tick;
    }

    if (is_down(IA_ZOOM_OUT)) {
        target_fovy += 1.0F * fovy_speed;
    }
    if (is_down(IA_ZOOM_IN)) {
        target_fovy -= 1.0F * fovy_speed;
    }

    // Clamp target to prevent extreme values
    target_fovy = glm::clamp(target_fovy, CAMERA3D_MIN_FOV, CAMERA3D_MAX_FOV);

    // Ease current fovy towards target fovy using exponential smoothing
    F32 const ease_factor = 1.0F - glm::exp(-zoom_ease_speed * dtu);
    F32 const new_fovy = glm::mix(c3d_get_fov(), target_fovy, ease_factor);
    c3d_set_fov(new_fovy);
    previous_cam_fovy = new_fovy;

    UpdateCameraPro(camera, movement, rotation, 0.0F);
}

void input_lighting_input_update() {
    if (is_pressed(IA_OPEN_OVERLAY_MENU)) { scenes_push_overlay_scene(SCENE_OVERLAY_MENU); }

    Camera3D const *camera = c3d_get_ptr();

    // First 5 lights are editable via input
    SZ const editable_lights_count = 5;

    for (SZ i = 0; i < editable_lights_count; ++i) {
        if (!is_pressed((IAction)(IA_DBG_LIGHT_1 + i))) { continue; }

        if (is_mod(I_MODIFIER_CTRL)) {
            lighting_set_light_position(i, camera->position);
        } else {
            Light *light = &g_lighting.lights[i];
            lighting_set_light_enabled(i, !light->enabled);
            Color color = {};
            color_from_vec4(&color, light->color);
            mio(TS("Light %zu: \\ouc{%s}%s", i, light->enabled ? "#00ff00ff" : "#ff0000ff", light->enabled ? " ON" : "OFF")->c, WHITE);
        }
    }

    for (SZ i = 0; i < editable_lights_count; ++i) {
        if (!is_pressed((IAction)(IA_DBG_TOGGLE_LIGHT_TYPE_1 + i))) { continue; }

        Light *light = &g_lighting.lights[i];

        LightType const new_type = (light->type == LIGHT_TYPE_POINT) ? LIGHT_TYPE_SPOT : LIGHT_TYPE_POINT;

        Color color = {};
        color_from_vec4(&color, light->color);
        Vector3 const camera_forward = Vector3Normalize(Vector3Subtract(camera->target, camera->position));

        switch (new_type) {
            case LIGHT_TYPE_POINT:
                lighting_set_point_light(i,
                                         light->enabled,
                                         light->position,
                                         color,
                                         light->intensity);
                mio(TS("Light %zu: POINT", i)->c, WHITE);
                break;
            case LIGHT_TYPE_SPOT:
                lighting_set_spot_light(i,
                                        light->enabled,
                                        light->position,
                                        camera_forward,
                                        color,
                                        light->intensity,
                                        math_cos_f32(DEG2RAD * 12.5F),
                                        math_cos_f32(DEG2RAD * 17.5F));
                mio(TS("Light %zu: SPOT", i)->c, WHITE);
                break;
        }
    }

    for (SZ i = 0; i < editable_lights_count; ++i) {
        if (!is_pressed((IAction)(IA_DBG_MOVE_LIGHT_1 + i))) { continue; }

        Light *light = &g_lighting.lights[i];

        Color color = {};
        color_from_vec4(&color, light->color);
        Vector3 const camera_forward = Vector3Normalize(Vector3Subtract(camera->target, camera->position));

        if (light->type == LIGHT_TYPE_SPOT) {
            lighting_set_spot_light(i,
                                    light->enabled,
                                    camera->position,
                                    camera_forward,
                                    color,
                                    light->intensity,
                                    math_cos_f32(DEG2RAD * 12.5F),
                                    math_cos_f32(DEG2RAD * 17.5F));
        } else {
            lighting_set_light_position(i, camera->position);
        }
        mio(TS("Light %zu moved to camera position", i)->c, WHITE);
    }
}

S32 input_get_main_gamepad_idx() {
    return i_input.main_gamepad_idx;
}

void input_play_mouse_tape(MouseTape *tape) {
    i_input.current_mouse_tape    = tape;
    i_input.is_playing_mouse_tape = true;
    i_input.mouse_tape_pos        = 0;
}

void input_pause_mouse_tape() {
    i_input.is_playing_mouse_tape = false;
}

void input_stop_mouse_tape() {
    i_input.current_mouse_tape    = nullptr;
    i_input.is_playing_mouse_tape = false;
    i_input.mouse_tape_pos        = 0;
}
