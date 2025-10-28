#pragma once

#include "common.hpp"

#include <raylib.h>

fwd_decl(String);
fwd_decl(AFont);
fwd_decl(ASound);
fwd_decl(Menu);

using MENU_CALLBACK_DATA = void *;
using MENU_CALLBACK_FUNC_YES = void (*)(MENU_CALLBACK_DATA);
using MENU_CALLBACK_FUNC_CHANGE = void (*)(MENU_CALLBACK_DATA, BOOL increase);
using MENU_STRING_GET_FUNC = C8 const *(*)(MENU_CALLBACK_DATA, SZ idx, Color *color_ptr, BOOL *color_override_ptr);

enum MenuEntryType : U8 {
    MENU_ENTRY_TYPE_LABEL,
    MENU_ENTRY_TYPE_DESC,
    MENU_ENTRY_TYPE_VALUE,
    MENU_ENTRY_TYPE_COUNT,
};

struct MenuBackgroundStyle {
    Color normal;
    Color shadow;
    Color border;
};

struct MenuEntryStyle {
    Color normal;
    Color selected;
    Color hovered;
};

struct MenuFontStyle {
    String *name;
    F32 size_perc;
};

struct MenuProperties {
    MenuBackgroundStyle label_bg;
    MenuBackgroundStyle desc_bg;
    MenuBackgroundStyle value_bg;

    MenuEntryStyle label_entry;
    MenuEntryStyle desc_entry;
    MenuEntryStyle value_entry;

    MenuFontStyle label_font;
    MenuFontStyle desc_font;
    MenuFontStyle value_font;

    F32 border_size_perc;
    F32 shadow_size_perc;
    F32 padding_between_entries_perc;
    F32 padding_between_label_and_desc_perc;
    F32 padding_between_entry_text_and_border_perc;
    F32 padding_between_label_and_value_perc;
    BOOL align_all_values_vertically;
    BOOL horizontal_selection_indicator;
    Color horizontal_selection_indicator_color;

    Vector2 center_perc;
    String *navigation_sound_name;
    String *selection_sound_name;
};

struct MenuEntryUpdateResult {
    Menu *menu;

    Rectangle label_text;
    Rectangle label_box;
    Color label_override_color;
    BOOL label_override_color_enabled;

    Rectangle desc_text;
    Rectangle desc_box;
    Color desc_override_color;
    BOOL desc_override_color_enabled;

    Rectangle value_text;
    Rectangle value_box;
    Color value_override_color;
    BOOL value_override_color_enabled;

    Rectangle entry_text;
};

struct MenuEntry {
    MenuEntryUpdateResult update_result;

    MENU_CALLBACK_DATA cb_data;
    MENU_CALLBACK_FUNC_YES func_yes;
    MENU_CALLBACK_FUNC_CHANGE func_change;
};

struct Menu {
    BOOL enabled;
    BOOL initialized;

    MenuProperties props;

    MENU_STRING_GET_FUNC label_get;
    MENU_CALLBACK_DATA label_data;

    MENU_STRING_GET_FUNC desc_get;
    MENU_CALLBACK_DATA desc_data;

    MENU_STRING_GET_FUNC value_get;
    MENU_CALLBACK_DATA value_data;

    Vector2 position_internal;  // This is updated every frame.
    Rectangle bounds_internal;  // This is updated every frame.
    F32 widest_label_internal;  // This is updated every frame.

    MenuEntry *entries;
    SZ entry_count;
    SZ selected_entry;
    SZ hovered_entry;

    AFont *debug_font;
};

void menu_init(Menu *menu, MenuEntry *entries, SZ entry_count, MenuProperties properties);
void menu_init_with_general_menu_defaults(Menu *menu,
                                          SZ entry_count,
                                          MENU_STRING_GET_FUNC label_getters,
                                          MENU_CALLBACK_DATA label_data,
                                          MENU_STRING_GET_FUNC desc_getters,
                                          MENU_CALLBACK_DATA desc_data,
                                          MENU_STRING_GET_FUNC value_getters,
                                          MENU_CALLBACK_DATA value_data,
                                          MENU_CALLBACK_FUNC_YES cb_yes,
                                          MENU_CALLBACK_FUNC_CHANGE cb_change,
                                          MENU_CALLBACK_DATA cb_data);
void menu_init_with_option_menu_defaults(Menu *menu,
                                         SZ entry_count,
                                         MENU_STRING_GET_FUNC label_getters,
                                         MENU_CALLBACK_DATA label_data,
                                         MENU_STRING_GET_FUNC desc_getters,
                                         MENU_CALLBACK_DATA desc_data,
                                         MENU_STRING_GET_FUNC value_getters,
                                         MENU_CALLBACK_DATA value_data,
                                         MENU_CALLBACK_FUNC_YES cb_yes,
                                         MENU_CALLBACK_FUNC_CHANGE cb_change,
                                         MENU_CALLBACK_DATA cb_data);
void menu_init_with_blackbox_menu_defaults(Menu *menu,
                                           SZ entry_count,
                                           MENU_STRING_GET_FUNC label_getters,
                                           MENU_CALLBACK_DATA label_data,
                                           MENU_STRING_GET_FUNC desc_getters,
                                           MENU_CALLBACK_DATA desc_data,
                                           MENU_STRING_GET_FUNC value_getters,
                                           MENU_CALLBACK_DATA value_data,
                                           MENU_CALLBACK_FUNC_YES cb_yes,
                                           MENU_CALLBACK_FUNC_CHANGE cb_change,
                                           MENU_CALLBACK_DATA cb_data);
MenuEntryUpdateResult menu_parse_entry(Menu *menu, SZ idx);
void menu_update(Menu *menu);
void menu_draw_2d_hud(Menu *menu);
void menu_draw_2d_dbg(Menu *menu);

// Helper stuff.
struct MenuExtraToggleData {
    BOOL *toggle;
    C8 const *name;
};

void menu_extra_toggle_data_cb_toggle(void *data);  // INFO: Expects MenuExtraToggleData
void menu_extra_override_static_color(Color *color_ptr, BOOL *color_override_ptr, Color color);
void menu_extra_override_toggle_color(Color *color_ptr, BOOL *color_override_ptr, BOOL toggle);
void menu_extra_override_lerp_color(Color *color_ptr, BOOL *color_override_ptr, Color a, Color b, F32 t);

// Menu.
SZ menu_extra_menu_func_get_entry_count();
C8 const *menu_extra_menu_func_get_label(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr);
C8 const *menu_extra_menu_func_get_desc(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr);
void menu_extra_menu_func_yes(void *_cb_data);

// Overworld.
SZ menu_extra_overworld_func_get_entry_count();
C8 const *menu_extra_overworld_func_get_label(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr);
C8 const *menu_extra_overworld_func_get_desc(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr);
C8 const *menu_extra_overworld_func_get_value(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr);
void menu_extra_overworld_func_yes(void *_cb_data);
void menu_extra_overworld_func_change(void *_cb_data, BOOL increase);

// Dungeon.
SZ menu_extra_dungeon_func_get_entry_count();
C8 const *menu_extra_dungeon_func_get_label(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr);
C8 const *menu_extra_dungeon_func_get_desc(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr);
C8 const *menu_extra_dungeon_func_get_value(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr);
void menu_extra_dungeon_func_yes(void *_cb_data);
void menu_extra_dungeon_func_change(void *_cb_data, BOOL increase);

// Option.
SZ menu_extra_option_func_get_entry_count();
C8 const *menu_extra_option_func_get_label(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr);
C8 const *menu_extra_option_func_get_desc(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr);
C8 const *menu_extra_option_func_get_value(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr);
void menu_extra_option_func_yes(void *_cb_data);
void menu_extra_option_func_change(void *_cb_data, BOOL increase);

// Overlay.
SZ menu_extra_overlay_func_get_entry_count();
C8 const *menu_extra_overlay_func_get_label(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr);
C8 const *menu_extra_overlay_func_get_desc(void *cb_data, SZ idx, Color *color_ptr, BOOL *color_override_ptr);
void menu_extra_overlay_func_yes(void *_cb_data);
