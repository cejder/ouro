#pragma once

#include "common.hpp"
#include "input.hpp"

// WARN: Currently, it is not allowed to "only sometimes" draw the dropdown since it will fuck up the index stuff.
// This whole implementation is janky and stupid but it should be fine since this is only for development.
//
// Just keep this in mind.

#define DBG_WIDGET_MAX 128
#define DBG_CALLBACK_PER_WINDOW_MAX 128
#define DBG_WINDOW_MIN_WIDTH 180
#define DBG_REF_CHECKBOX_SIZE {20.0F, 20.0F}
#define DBG_REF_BUTTON_SIZE {DBG_WINDOW_MIN_WIDTH, 20.0F}
#define DBG_REF_DROPDOWN_SIZE {DBG_WINDOW_MIN_WIDTH / 2.0F, 20.0F}
#define DBG_REF_FILLBAR_SIZE {DBG_WINDOW_MIN_WIDTH, 20.0F}

#define DBG_INI_FILE_PATH "debug.ini"
#define DBG_NO_WID S32_MAX
#define DBG_ARENA_ALLOCATIONS_TILL_RESET 1000000
#define DBG_MAX_RANDOM_COLOR_ATTEMPTS 16
#define DBG_FPS_FLUCTUATION_SAMPLE_COUNT 128
#define DBG_CLOSE_GRACE_PERIOD_START 0.33F

#define DBG_WINDOW_BG_COLOR BASICALLYBLACK
#define DBG_WINDOW_FG_COLOR NAYBEIGE
#define DBG_FILLBAR_BG_COLOR NAYGREEN
#define DBG_FILLBAR_FG_COLOR EVAFATPURPLE
#define DBG_FILLBAR_TEXT_COLOR NAYBEIGE

#define DBG_TIMELINE_HEIGHT 32
#define DBG_WIDGET_ELEMENT_TO_TEXT_PADDING DBG_WIDGET_PADDING
#define DBG_WIDGET_PADDING 5
#define DBG_WINDOW_BORDER_SIZE 2
#define DBG_WINDOW_INNER_PADDING 5
#define DBG_WINDOW_TITLE_BAR_HEIGHT ((F32)c_debug__medium_font_size + DBG_WINDOW_INNER_PADDING)

fwd_decl(DBGWidget);
fwd_decl(AFont);
fwd_decl(ATexture);
fwd_decl(INIFile);

struct DBGWidgetUpdatePackage {
    DBGWidget *widget;
    Rectangle *window_rec;
    Vector2 mouse_pos;
    Vector2 mouse_delta;
    BOOL is_mouse_down[I_MOUSE_COUNT];
    BOOL is_mouse_pressed[I_MOUSE_COUNT];
    BOOL is_mouse_released[I_MOUSE_COUNT];
};

struct DBGWidgetDrawPackage {
    DBGWidget *widget;
};

using DBG_WIDGET_UPDATE_FUNC = void (*)(DBGWidgetUpdatePackage *p);
using DBG_WIDGET_DRAW_FUNC = void (*)(DBGWidgetDrawPackage *p);

using DBG_CALLBACK_FUNC = void (*)(void *data);
using DBG_CALLBACK_DATA = void *;

#define DBG_WIDGET_CB_NONE \
    (DBGWidgetCallback) {}

struct DBGWidgetCallback {
    DBG_CALLBACK_FUNC func;
    DBG_CALLBACK_DATA data;
};

enum DBGWidgetDataType : U8 {
    DBG_WIDGET_DATA_TYPE_FLOAT,
    DBG_WIDGET_DATA_TYPE_INT,
    DBG_WIDGET_DATA_TYPE_U8,
};

enum DBGWidgetMouseState : U8 {
    DBG_WIDGET_MOUSE_STATE_IDLE,
    DBG_WIDGET_MOUSE_STATE_HOVER,
    DBG_WIDGET_MOUSE_STATE_CLICK,
    DBG_WIDGET_MOUSE_STATE_COUNT
};

struct DBGWidgetSeparator {
    F32 height;
};

void dbg_widget_separator_add(F32 height);
void dbg_widget_separator_update(DBGWidgetUpdatePackage *p);
void dbg_widget_separator_draw(DBGWidgetDrawPackage *p);

struct DBGWidgetLabel {
    C8 const *text;
    AFont *font;
    Color text_color;
    BOOL is_ouc;
    BOOL has_shadow;
};

void dbg_widget_label_add(C8 const *text,
                          AFont *font,
                          Color text_color,
                          BOOL is_ouc,
                          BOOL has_shadow);
void dbg_widget_label_update(DBGWidgetUpdatePackage *p);
void dbg_widget_label_draw(DBGWidgetDrawPackage *p);

struct DBGWidgetButton {
    C8 const *text;
    AFont *font;
    Color text_color;
    Vector2 min_button_size;
    DBGWidgetMouseState mouse_state;
    DBGWidgetCallback on_click_cb;
};

void dbg_widget_button_add(C8 const *text,
                           AFont *font,
                           Color text_color,
                           Vector2 min_button_size,
                           DBGWidgetCallback on_click_cb);
void dbg_widget_button_update(DBGWidgetUpdatePackage *p);
void dbg_widget_button_draw(DBGWidgetDrawPackage *p);

struct DBGWidgetCheckbox {
    C8 const *text;
    AFont *font;
    Color text_color;
    Vector2 checkbox_size;
    BOOL *check_ptr;
    DBGWidgetMouseState mouse_state;
    DBGWidgetCallback on_click_cb;
};

void dbg_widget_checkbox_add(C8 const *text,
                             AFont *font,
                             Color text_color,
                             Vector2 checkbox_size,
                             BOOL *check_ptr,
                             DBGWidgetCallback on_click_cb);
void dbg_widget_checkbox_update(DBGWidgetUpdatePackage *p);
void dbg_widget_checkbox_draw(DBGWidgetDrawPackage *p);

#define DBG_WIDGET_DROPDOWN_IDX_NONE SZ_MAX
#define DBG_WIDGET_DROPDOWN_ITEM_IDX_NONE SZ_MAX

struct DBGWidgetDropdown {
    SZ dropdown_idx;
    BOOL is_open;
    C8 const *text;
    AFont *font;
    Color text_color;
    Color item_color;
    Color hovered_item_color;
    Vector2 dropdown_size;
    C8 const **dropdown_items;
    SZ *selected_idx;
    SZ dropdown_item_count;
    SZ dropdown_item_hovered_idx;
    DBGWidgetMouseState mouse_state;
    DBGWidgetCallback on_click_cb;
};

void dbg_widget_dropdown_add(C8 const *text,
                             AFont *font,
                             Color text_color,
                             Color item_color,
                             Color hovered_item_color,
                             Vector2 min_dropdown_size,
                             C8 const **dropdown_items,
                             SZ dropdown_item_count,
                             SZ *selected_idx,
                             DBGWidgetCallback on_click_cb);
void dbg_widget_dropdown_update(DBGWidgetUpdatePackage *p);
void dbg_widget_dropdown_draw(DBGWidgetDrawPackage *p);

struct DBGWidgetFillbar {
    C8 const *text;
    AFont *font;
    Color fill_color;
    Color empty_color;
    Color text_color;
    Vector2 fillbar_size;
    F32 current_value;
    F32 min_value;
    F32 max_value;
    BOOL is_slider;
    void *slider_value;
    DBGWidgetDataType slider_data_type;
    DBGWidgetCallback on_click_cb;
};

void dbg_widget_fillbar_add(C8 const *text,
                            AFont *font,
                            Color fill_color,
                            Color empty_color,
                            Color text_color,
                            Vector2 fillbar_size,
                            F32 current_value,
                            F32 min_value,
                            F32 max_value,
                            BOOL is_slider,
                            void *slider_value,
                            DBGWidgetDataType slider_data_type,
                            DBGWidgetCallback on_click_cb);
void dbg_widget_fillbar_update(DBGWidgetUpdatePackage *p);
void dbg_widget_fillbar_draw(DBGWidgetDrawPackage *p);

struct DBGWidgetTimeline {
    Color foreground_color;
    Color background_color;
    F32 *data_ptr;
    SZ data_count;
    F32 min_value;
    F32 max_value;
    C8 const *fmt;
    C8 const *unit;
};

void dbg_widget_timeline_add(Color foreground_color,
                             Color background_color,
                             F32 *data_ptr,
                             SZ data_count,
                             F32 min_value,
                             F32 max_value,
                             C8 const *fmt,
                             C8 const *unit);
void dbg_widget_timeline_update(DBGWidgetUpdatePackage *p);
void dbg_widget_timeline_draw(DBGWidgetDrawPackage *p);

struct DBGWidgetTexture {
    C8 const *text;
    RenderTexture *texture;
    Vector2 texture_size;
    Color highlight_color;
    Color texture_tint;
    BOOL want_big_view;
    BOOL want_depth;
};

void dbg_widget_texture_add(C8 const *text,
                            RenderTexture *texture,
                            Vector2 texture_size,
                            Color highlight_color,
                            Color texture_tint);
void dbg_widget_texture_update(DBGWidgetUpdatePackage *p);
void dbg_widget_texture_draw(DBGWidgetDrawPackage *p);

struct DBGWidgetColorView {
    C8 const *text;
    Color *color_ptr;
    Vector2 size;
    BOOL want_big_view;
    Rectangle big_view_src;
    Rectangle big_view_dst;
    DBGWidgetCallback on_color_change_cb;
};

void dbg_widget_color_view_add(C8 const *text,
                               Color *color_ptr,
                               Vector2 size,
                               DBGWidgetCallback on_color_change_cb);
void dbg_widget_color_view_update(DBGWidgetUpdatePackage *p);
void dbg_widget_color_view_draw(DBGWidgetDrawPackage *p);

enum DBGWidgetType : U8 {
    DBG_WIDGET_TYPE_SEPARATOR,
    DBG_WIDGET_TYPE_LABEL,
    DBG_WIDGET_TYPE_BUTTON,
    DBG_WIDGET_TYPE_CHECKBOX,
    DBG_WIDGET_TYPE_DROPDOWN,
    DBG_WIDGET_TYPE_FILLBAR,
    DBG_WIDGET_TYPE_TIMELINE,
    DBG_WIDGET_TYPE_TEXTURE,
    DBG_WIDGET_TYPE_COLOR_VIEW,
};

struct DBGWidget {
    DBG_WIDGET_UPDATE_FUNC update_func;
    DBG_WIDGET_DRAW_FUNC draw_func;
    Rectangle bounds;
    DBGWidgetType type;
    union {
        DBGWidgetSeparator separator;
        DBGWidgetLabel label;
        DBGWidgetButton button;
        DBGWidgetCheckbox checkbox;
        DBGWidgetDropdown dropdown;
        DBGWidgetFillbar fillbar;
        DBGWidgetTimeline timeline;
        DBGWidgetTexture texture;
        DBGWidgetColorView color_view;
    };
};

#define dwnd(wid) \
dbg_set_active_window(wid); if (!(g_dbg.windows[wid].flags & DBG_WFLAG_COLLAPSED))

#define dwis(height) \
dbg_widget_separator_add(height)

#define dwil(text, font, text_c) \
dbg_widget_label_add(text, font, text_c, false, false)

#define dwilo(text, font, text_c) \
dbg_widget_label_add(text, font, text_c, true, false)

#define dwils(text, font, text_c) \
dbg_widget_label_add(text, font, text_c, false, true)

#define dwilos(text, font, text_c) \
dbg_widget_label_add(text, font, text_c, true, true)

#define dwib(text, font, text_c, min_button_size, on_click_func, on_click_data) \
dbg_widget_button_add(text, font, text_c, min_button_size, {on_click_func, on_click_data})

#define dwicb(text, font, text_c, checkbox_size, check_ptr, on_click_func, on_click_data) \
dbg_widget_checkbox_add(text, font, text_c, checkbox_size, check_ptr, {on_click_func, on_click_data})

#define dwidd(text, font, text_c, item_c, hov_item_c, min_dd_size, items, item_count, sel_idx_ptr, on_click_func, on_click_data) \
dbg_widget_dropdown_add(text, font, text_c, item_c, hov_item_c, min_dd_size, items, item_count, sel_idx_ptr, {on_click_func, on_click_data});

#define dwifb(text, font, fill_c, empty_c, text_c, fillbar_size, current_v, min_v, max_v, is_slider, slider_v, slider_data_type, on_click_data) \
dbg_widget_fillbar_add(text, font, fill_c, empty_c, text_c, fillbar_size, current_v, min_v, max_v, is_slider, slider_v, slider_data_type, on_click_data)

#define dwitl(fg_c, bg_c, data_ptr, data_count, min_v, max_v, fmt, unit) \
dbg_widget_timeline_add(fg_c, bg_c, data_ptr, data_count, min_v, max_v, fmt, unit)

#define dwitx(text, texture, texture_size, highlight_c, text_tint) \
dbg_widget_texture_add(text, texture, texture_size, highlight_c, text_tint)

#define dwicv(t, c, s, cb) \
dbg_widget_color_view_add(t, c, s, cb)

enum DBGWID : U8 {
    DBG_WID_RENDER,
    DBG_WID_MEMORY,
    DBG_WID_SCENE,
    DBG_WID_WORLD,
    DBG_WID_ASSET,
    DBG_WID_OPTIONS,
    DBG_WID_PERFORMANCE,
    DBG_WID_AUDIO,
    DBG_WID_TARGETS,
    DBG_WID_CUSTOM_0,
    DBG_WID_CUSTOM_1,
    DBG_WID_COUNT,
};

enum DBGWFlag : U8 {
    DBG_WFLAG_COLLAPSED = 1 << 0,
};

struct DBGWindow {
    S32 flags;
    Rectangle bounds;
    DBGWidget widgets[DBG_WIDGET_MAX];
    SZ widget_count;
    DBGWidgetCallback cbs[DBG_CALLBACK_PER_WINDOW_MAX];
    SZ cb;
};

struct DBG {
    BOOL initialized;
    INIFile *ini;
    AFont *small_font;
    AFont *medium_font;
    AFont *large_font;
    S32 dragging_wid;
    S32 last_clicked_wid;
    BOOL modal_overlay_active;
    DBGWID active_wid;
    DBGWindow windows[DBG_WID_COUNT];
    DBGWID window_order[DBG_WID_COUNT];
    SZ last_widget_count;
    SZ current_dropdown_idx;
    SZ active_dropdown_idx;
    F32 dropdown_close_grace_period;
    Color color_view_last_color;
    Color color_view_copied_color;
    Image color_view_selection_image;
    Texture2D color_view_selection_texture;
    Texture2D color_view_clean_texture;
};

DBG extern g_dbg;

void dbg_init();
void dbg_quit();
void dbg_update();
void dbg_draw();
void dbg_add_cb(DBGWID wid, DBGWidgetCallback cb);
void dbg_remove_cb(DBGWID wid, DBGWidgetCallback cb);
void dbg_set_active_window(DBGWID wid);
void dbg_reset_windows(BOOL everything);
void dbg_load_ini();
void dbg_save_ini();
Color dbg_get_color_at_color_view(Vector2 pos, Rectangle color_view_bounds);
BOOL dbg_is_mouse_over_or_dragged();
