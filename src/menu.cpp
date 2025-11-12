#include "menu.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "audio.hpp"
#include "color.hpp"
#include "console.hpp"
#include "cvar.hpp"
#include "debug.hpp"
#include "input.hpp"
#include "math.hpp"
#include "memory.hpp"
#include "message.hpp"
#include "profiler.hpp"
#include "render.hpp"
#include "string.hpp"
#include "particles_2d.hpp"

#include <raymath.h>

void menu_init(Menu *menu, MenuEntry *entries, SZ entry_count, MenuProperties properties) {
    _assert_(entry_count > 0, "Menu must have at least one entry");

    menu->entries        = entries;
    menu->entry_count    = entry_count;
    menu->props          = properties;
    menu->selected_entry = 0;
    menu->hovered_entry  = SZ_MAX;
    menu->initialized    = true;
    menu->enabled        = true;

    menu->debug_font = asset_get_font("spleen-6x12", 12);
}

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
                                          MENU_CALLBACK_DATA cb_data) {
    // GENERAL:
    MenuProperties menu_properties = {};
    menu_properties.border_size_perc                           = 0.0F;
    menu_properties.shadow_size_perc                           = 0.0F;
    menu_properties.padding_between_entries_perc               = 0.0F;
    menu_properties.padding_between_label_and_desc_perc        = 0.0F;
    menu_properties.padding_between_entry_text_and_border_perc = 0.0F;
    menu_properties.padding_between_label_and_value_perc       = 0.0F;
    menu_properties.align_all_values_vertically                = true;
    menu_properties.horizontal_selection_indicator             = true;
    menu_properties.horizontal_selection_indicator_color       = PERSIMMON;
    menu_properties.center_perc                                = {0.80F, 0.50F};
    menu_properties.navigation_sound_name                      = PS("menu_movement.ogg");
    menu_properties.selection_sound_name                       = PS("menu_selection.ogg");

    // LABEL:
    // - background.
    MenuBackgroundStyle label_background_style = {};
    label_background_style.normal = BLANK;
    label_background_style.shadow = BLANK;
    label_background_style.border = BLANK;
    menu_properties.label_bg      = label_background_style;
    // - entry.
    MenuEntryStyle label_entry_style = {};
    label_entry_style.normal    = NAYBEIGE;
    label_entry_style.selected  = PERSIMMON;
    label_entry_style.hovered   = ORANGE;
    menu_properties.label_entry = label_entry_style;
    // - font.
    menu_properties.label_font  = {PS("GoMono"), 4.5F};

    // DESCRIPTION:
    // - background.
    MenuBackgroundStyle desc_background_style = {};
    desc_background_style.normal = BLANK;
    desc_background_style.shadow = BLANK;
    desc_background_style.border = BLANK;
    menu_properties.desc_bg      = desc_background_style;
    // - entry.
    MenuEntryStyle desc_entry_style = {};
    desc_entry_style.normal    = RAYWHITE;
    desc_entry_style.selected  = ORANGE;
    desc_entry_style.hovered   = LIGHTGRAY;
    menu_properties.desc_entry = desc_entry_style;
    // - font.
    menu_properties.desc_font  = {PS("GoMono"), 3.5F};

    // VALUE:
    // - background.
    MenuBackgroundStyle value_background_style = {};
    value_background_style.normal = BLANK;
    value_background_style.shadow = BLANK;
    value_background_style.border = BLANK;
    menu_properties.value_bg      = value_background_style;
    // - entry.
    MenuEntryStyle value_entry_style = {};
    value_entry_style.normal    = BEIGE;
    value_entry_style.selected  = GOLD;
    value_entry_style.hovered   = LIGHTGRAY;
    menu_properties.value_entry = value_entry_style;
    // - font.
    menu_properties.value_font  = {PS("GoMono"), 4.5F};

    auto *entries = mmpa(MenuEntry *, sizeof(MenuEntry) * entry_count);
    for (SZ i = 0; i < entry_count; ++i) {
        entries[i].cb_data     = cb_data;
        entries[i].func_yes    = cb_yes;
        entries[i].func_change = cb_change;
    }

    menu->label_get  = label_getters;
    menu->label_data = label_data;
    menu->desc_get   = desc_getters;
    menu->desc_data  = desc_data;
    menu->value_get  = value_getters;
    menu->value_data = value_data;

    menu_init(menu, entries, entry_count, menu_properties);
}

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
                                         MENU_CALLBACK_DATA cb_data) {
    // GENERAL:
    MenuProperties menu_properties = {};
    menu_properties.border_size_perc                           = 0.0F;
    menu_properties.shadow_size_perc                           = 0.0F;
    menu_properties.padding_between_entries_perc               = 0.0F;
    menu_properties.padding_between_label_and_desc_perc        = 0.0F;
    menu_properties.padding_between_entry_text_and_border_perc = 0.0F;
    menu_properties.padding_between_label_and_value_perc       = 1.0F;
    menu_properties.align_all_values_vertically                = true;
    menu_properties.horizontal_selection_indicator             = true;
    menu_properties.horizontal_selection_indicator_color       = PERSIMMON;
    menu_properties.center_perc                                = {0.80F, 0.515F};
    menu_properties.navigation_sound_name                      = PS("menu_movement.ogg");
    menu_properties.selection_sound_name                       = PS("menu_selection.ogg");

    // LABEL:
    // - background.
    MenuBackgroundStyle label_background_style = {};
    label_background_style.normal = BLANK;
    label_background_style.shadow = BLANK;
    label_background_style.border = BLANK;
    menu_properties.label_bg      = label_background_style;
    // - entry.
    MenuEntryStyle label_entry_style = {};
    label_entry_style.normal    = NAYBEIGE;
    label_entry_style.selected  = PERSIMMON;
    label_entry_style.hovered   = ORANGE;
    menu_properties.label_entry = label_entry_style;
    // - font.
    menu_properties.label_font  = {PS("GoMono"), 3.5F};

    // DESCRIPTION:
    // - background.
    MenuBackgroundStyle desc_background_style = {};
    desc_background_style.normal = BLANK;
    desc_background_style.shadow = BLANK;
    desc_background_style.border = BLANK;
    menu_properties.desc_bg      = desc_background_style;
    // - entry.
    MenuEntryStyle desc_entry_style = {};
    desc_entry_style.normal    = WHITE;
    desc_entry_style.selected  = WHITE;
    desc_entry_style.hovered   = WHITE;
    menu_properties.desc_entry = desc_entry_style;
    // - font.
    menu_properties.desc_font  = {PS("GoMono"), 2.5F};

    // VALUE:
    // - background.
    MenuBackgroundStyle value_background_style = {};
    value_background_style.normal = BLANK;
    value_background_style.shadow = BLANK;
    value_background_style.border = BLANK;
    menu_properties.value_bg      = value_background_style;
    // - entry.
    MenuEntryStyle value_entry_style = {};
    value_entry_style.normal    = color_darken(NAYBEIGE, 0.5F);
    value_entry_style.selected  = PERSIMMON;
    value_entry_style.hovered   = ORANGE;
    menu_properties.value_entry = value_entry_style;
    // - font.
    menu_properties.value_font  = {PS("GoMono"), 3.5F};

    auto *entries = mmpa(MenuEntry *, sizeof(MenuEntry) * entry_count);
    for (SZ i = 0; i < entry_count; ++i) {
        entries[i].cb_data     = cb_data;
        entries[i].func_yes    = cb_yes;
        entries[i].func_change = cb_change;
    }

    menu->label_get  = label_getters;
    menu->label_data = label_data;
    menu->desc_get   = desc_getters;
    menu->desc_data  = desc_data;
    menu->value_get  = value_getters;
    menu->value_data = value_data;

    menu_init(menu, entries, entry_count, menu_properties);
}

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
                                           MENU_CALLBACK_DATA cb_data) {
    // GENERAL:
    MenuProperties menu_properties = {};
    menu_properties.border_size_perc                           = 0.25F;
    menu_properties.shadow_size_perc                           = 0.0F;
    menu_properties.padding_between_entries_perc               = menu_properties.border_size_perc * 3.0F;
    menu_properties.padding_between_label_and_desc_perc        = 0.0F;
    menu_properties.padding_between_entry_text_and_border_perc = 0.0F;
    menu_properties.padding_between_label_and_value_perc       = menu_properties.border_size_perc * 3.0F;
    menu_properties.align_all_values_vertically                = true;
    menu_properties.center_perc                                = {0.75F, 0.50F};
    menu_properties.navigation_sound_name                      = PS("menu_movement.ogg");
    menu_properties.selection_sound_name                       = PS("menu_selection.ogg");

    // LABEL:
    // - background.
    MenuBackgroundStyle label_background_style = {};
    label_background_style.normal = color_darken(MAROON, 0.85F);
    label_background_style.shadow = BLANK;
    label_background_style.border = color_darken(MAROON, 0.85F);
    menu_properties.label_bg      = label_background_style;
    // - entry.
    MenuEntryStyle label_entry_style = {};
    label_entry_style.normal    = PERSIMMON;
    label_entry_style.selected  = WHITE;
    label_entry_style.hovered   = GOLD;
    menu_properties.label_entry = label_entry_style;
    // - font.
    menu_properties.label_font  = {PS("GoMono"), 2.0F};

    // DESCRIPTION:
    // - background.
    MenuBackgroundStyle desc_background_style = {};
    desc_background_style.normal = NEARBLACK;
    desc_background_style.shadow = BLANK;
    desc_background_style.border = NEARBLACK;
    menu_properties.desc_bg      = desc_background_style;
    // - entry.
    MenuEntryStyle desc_entry_style = {};
    desc_entry_style.normal    = DARKGRAY;
    desc_entry_style.selected  = WHITE;
    desc_entry_style.hovered   = GOLD;
    menu_properties.desc_entry = desc_entry_style;
    // - font.
    menu_properties.desc_font  = {PS("GoMono"), 1.75F};

    // VALUE:
    // - background.
    MenuBackgroundStyle value_background_style = {};
    value_background_style.normal = color_darken(MAROON, 0.65F);
    value_background_style.shadow = BLANK;
    value_background_style.border = color_darken(MAROON, 0.65F);
    menu_properties.value_bg      = value_background_style;
    // - entry.
    MenuEntryStyle value_entry_style = {};
    value_entry_style.normal    = BEIGE;
    value_entry_style.selected  = WHITE;
    value_entry_style.hovered   = GOLD;
    menu_properties.value_entry = value_entry_style;
    // - font.
    menu_properties.value_font  = {PS("GoMono"), 2.0F};

    auto *entries = mmpa(MenuEntry *, sizeof(MenuEntry) * entry_count);
    for (SZ i = 0; i < entry_count; ++i) {
        entries[i].cb_data     = cb_data;
        entries[i].func_yes    = cb_yes;
        entries[i].func_change = cb_change;
    }

    menu->label_get  = label_getters;
    menu->label_data = label_data;
    menu->desc_get   = desc_getters;
    menu->desc_data  = desc_data;
    menu->value_get  = value_getters;
    menu->value_data = value_data;

    menu_init(menu, entries, entry_count, menu_properties);
}

MenuEntryUpdateResult menu_parse_entry(Menu *menu, SZ idx) {
    MenuEntryUpdateResult r = {};

    Vector2 const res = {(F32)c_video__render_resolution_width, (F32)c_video__render_resolution_height};

    AFont *label_font = asset_get_font(menu->props.label_font.name->c, ui_font_size(menu->props.label_font.size_perc));
    AFont *desc_font  = asset_get_font(menu->props.desc_font.name->c, ui_font_size(menu->props.desc_font.size_perc));
    AFont *value_font = asset_get_font(menu->props.value_font.name->c, ui_font_size(menu->props.value_font.size_perc));

    // Remove usage of 'res' variable
    (void)res;

    Vector2 const label_dim = measure_text(label_font, menu->label_get(menu->label_data, idx, &r.label_override_color, &r.label_override_color_enabled));
    Vector2 const desc_dim  = measure_text(desc_font, menu->desc_get(menu->desc_data, idx, &r.desc_override_color, &r.desc_override_color_enabled));
    F32 const text_pox_y    = menu->position_internal.y +
        ((F32)idx * (label_dim.y + desc_dim.y + ui_scale_x(menu->props.padding_between_entries_perc) + ui_scale_x(menu->props.padding_between_label_and_desc_perc)));

    r.menu = menu;

    F32 const pad_entry_text_border = ui_scale_x(menu->props.padding_between_entry_text_and_border_perc);

    r.label_text.x      = menu->position_internal.x - pad_entry_text_border;
    r.label_text.y      = text_pox_y - pad_entry_text_border;
    r.label_text.width  = label_dim.x;
    r.label_text.height = label_dim.y;

    r.desc_text.x      = r.label_text.x;
    r.desc_text.y      = r.label_text.y + ui_scale_x(menu->props.padding_between_label_and_desc_perc) + r.label_text.height;
    r.desc_text.width  = desc_dim.x;
    r.desc_text.height = desc_dim.y;

    r.entry_text.x      = glm::min(r.label_text.x, r.desc_text.x);
    r.entry_text.y      = glm::min(r.label_text.y, r.desc_text.y);
    r.entry_text.width  = glm::max(r.label_text.width, r.desc_text.width);
    r.entry_text.height = r.label_text.height + r.desc_text.height;

    // Value is optional.
    C8 const *value = nullptr;
    if (menu->value_get) {
        value = menu->value_get(menu->value_data, idx, &r.value_override_color, &r.value_override_color_enabled);
        if (value) {
            Vector2 const value_dim = measure_text(value_font, value);
            r.value_text.x = menu->props.align_all_values_vertically
                                 ? r.label_text.x + menu->widest_label_internal + ui_scale_x(menu->props.padding_between_label_and_value_perc)
                                 : r.label_text.x + r.label_text.width + ui_scale_x(menu->props.padding_between_label_and_value_perc);
            r.value_text.y      = r.label_text.y;
            r.value_text.width  = value_dim.x;
            r.value_text.height = value_dim.y;

            // We also need to update the entry_rec width.
            r.entry_text.width =
                glm::max(r.label_text.width + r.value_text.width + ui_scale_x(menu->props.padding_between_label_and_value_perc), r.desc_text.width);
        }
    }

    r.label_box.x      = r.label_text.x - ui_scale_x(menu->props.padding_between_entry_text_and_border_perc);
    r.label_box.y      = r.label_text.y - ui_scale_x(menu->props.padding_between_entry_text_and_border_perc);
    r.label_box.width  = r.label_text.width + (2 * ui_scale_x(menu->props.padding_between_entry_text_and_border_perc));
    r.label_box.height = r.label_text.height + (2 * ui_scale_x(menu->props.padding_between_entry_text_and_border_perc));

    r.desc_box.x      = r.desc_text.x - ui_scale_x(menu->props.padding_between_entry_text_and_border_perc);
    r.desc_box.y      = r.desc_text.y - ui_scale_x(menu->props.padding_between_entry_text_and_border_perc);
    r.desc_box.width  = r.desc_text.width + (2 * ui_scale_x(menu->props.padding_between_entry_text_and_border_perc));
    r.desc_box.height = r.desc_text.height + (2 * ui_scale_x(menu->props.padding_between_entry_text_and_border_perc));

    if (menu->value_get && value) {
        r.value_box.x      = r.value_text.x - ui_scale_x(menu->props.padding_between_entry_text_and_border_perc);
        r.value_box.y      = r.value_text.y - ui_scale_x(menu->props.padding_between_entry_text_and_border_perc);
        r.value_box.width  = r.value_text.width + ui_scale_x(2 * menu->props.padding_between_entry_text_and_border_perc);
        r.value_box.height = r.value_text.height + ui_scale_x(2 * menu->props.padding_between_entry_text_and_border_perc);
    }

    return r;
}

void menu_update(Menu *menu) {
    if (!menu->enabled) { return; }

    _assert_(menu->initialized, "Menu must be initialized before updating");
    _assert_(menu->entry_count > 0, "Menu must have at least one entry");

    AFont *label_font = asset_get_font(menu->props.label_font.name->c, ui_font_size(menu->props.label_font.size_perc));
    AFont *desc_font  = asset_get_font(menu->props.desc_font.name->c, ui_font_size(menu->props.desc_font.size_perc));
    AFont *value_font = asset_get_font(menu->props.value_font.name->c, ui_font_size(menu->props.value_font.size_perc));

    menu->bounds_internal       = {};  // Reset every frame.
    menu->widest_label_internal = 0.0F;

    // Dummy variables for text measurement - values unused
    Color temp_color;
    BOOL temp_override = false;

    // Calculate widest label for value alignment if needed
    if (menu->value_get && menu->props.align_all_values_vertically) {
        for (SZ i = 0; i < menu->entry_count; i++) {
            Vector2 const label_dim = measure_text(label_font, menu->label_get(menu->label_data, i, &temp_color, &temp_override));
            menu->widest_label_internal = glm::max(menu->widest_label_internal, label_dim.x);
        }
    }

    // Calculate bounds
    for (SZ i = 0; i < menu->entry_count; i++) {
        Vector2 const label_dim = measure_text(label_font, menu->label_get(menu->label_data, i, &temp_color, &temp_override));
        Vector2 const desc_dim  = measure_text(desc_font, menu->desc_get(menu->desc_data, i, &temp_color, &temp_override));

        F32 entry_width = label_dim.x;

        if (menu->value_get) {
            C8 const *value = menu->value_get(menu->value_data, i, &temp_color, &temp_override);
            if (value) {
                Vector2 const value_dim = measure_text(value_font, value);
                if (menu->props.align_all_values_vertically) {
                    entry_width = menu->widest_label_internal + ui_scale_x(menu->props.padding_between_label_and_value_perc) + value_dim.x;
                } else {
                    entry_width += ui_scale_x(menu->props.padding_between_label_and_value_perc) + value_dim.x;
                }
            }
        }

        menu->bounds_internal.width = glm::max(menu->bounds_internal.width, entry_width);
        menu->bounds_internal.width = glm::max(menu->bounds_internal.width, desc_dim.x);
        menu->bounds_internal.height +=
            label_dim.y + desc_dim.y + ui_scale_x(menu->props.padding_between_entries_perc) + ui_scale_x(menu->props.padding_between_label_and_desc_perc);
    }

    Vector2 const res = render_get_render_resolution();
    menu->position_internal = Vector2Subtract(Vector2Multiply(menu->props.center_perc, res), Vector2{menu->bounds_internal.width / 2.0F, menu->bounds_internal.height / 2.0F});

    // NOTE: We only check for movement if the debug console is not open.

    BOOL no_entry_hovered     = true;

    if (!c_console__enabled) {
        BOOL made_movement = false;

        // NOTE: clang-tidy does not care about the menu->_assert_ above checking for menu->_entry_count > 0.
        // It's still nagging here since we might divide by 0 here. We know that this is not the case
        // but we still need to tell clang-tidy that we know what we are doing.
        if (menu->entry_count > 0) {
            if (is_pressed_or_repeat(IA_PREVIOUS)) {
                menu->selected_entry = (menu->selected_entry - 1 + menu->entry_count) % menu->entry_count;
                made_movement = true;
            } else if (is_pressed_or_repeat(IA_NEXT)) {
                menu->selected_entry = (menu->selected_entry + 1) % menu->entry_count;
                made_movement = true;
            }
        }

        if (made_movement) {
            audio_reset(ACG_SFX);
            audio_play(ACG_SFX, menu->props.navigation_sound_name->c);
        }

        if (is_pressed(IA_YES)) {
            MenuEntry *selected_entry = &menu->entries[menu->selected_entry];
            if (selected_entry->func_yes) {
                selected_entry->func_yes(selected_entry->cb_data);
                audio_reset(ACG_SFX);
                audio_play(ACG_SFX, menu->props.selection_sound_name->c);
            }
        }

        // If there is no value, we don't need to do anything.
        if (is_pressed_or_repeat(IA_DECREASE)) {
            MenuEntry *selected_entry = &menu->entries[menu->selected_entry];
            if (selected_entry->func_change) {
                selected_entry->func_change(selected_entry->cb_data, false);
                audio_reset(ACG_SFX);
                audio_play(ACG_SFX, menu->props.navigation_sound_name->c);
            }
        }

        if (is_pressed_or_repeat(IA_INCREASE)) {
            MenuEntry *selected_entry = &menu->entries[menu->selected_entry];
            if (selected_entry->func_change) {
                selected_entry->func_change(selected_entry->cb_data, true);
                audio_reset(ACG_SFX);
                audio_play(ACG_SFX, menu->props.navigation_sound_name->c);
            }
        }

        Vector2 const mouse_position = input_get_mouse_position();

        for (SZ i = 0; i < menu->entry_count; i++) {
            MenuEntry *entry = &menu->entries[i];

            if (CheckCollisionPointRec(mouse_position, entry->update_result.label_box) ||
                CheckCollisionPointRec(mouse_position, entry->update_result.desc_box) ||
                CheckCollisionPointRec(mouse_position, entry->update_result.value_box)) {
                menu->hovered_entry = i;
                no_entry_hovered = false;

                if (is_mouse_pressed(I_MOUSE_LEFT) && !dbg_is_mouse_over_or_dragged()) {
                    audio_reset(ACG_SFX);
                    audio_play(ACG_SFX, menu->props.selection_sound_name->c);

                    menu->selected_entry = i;
                    if (entry->func_yes) { entry->func_yes(entry->cb_data); }

                    auto start_color = GOLD;
                    auto end_color   = MAROON;

                    Rectangle const target_rect = {
                        mouse_position.x - 2.5F, mouse_position.y - 2.5F,
                        5.0F, 5.0F,
                    };

                    particles2d_queue_fire(target_rect, start_color, end_color, 2.0F, 5);
                }
            }
        }
    }

    menu->bounds_internal.x = menu->position_internal.x;
    menu->bounds_internal.y = menu->position_internal.y;

    if (no_entry_hovered) { menu->hovered_entry = SZ_MAX; }

    for (SZ i = 0; i < menu->entry_count; i++) {
        MenuEntry *entry = &menu->entries[i];
        entry->update_result = menu_parse_entry(menu, i);
    }
}

void static i_draw_entry(MenuEntryUpdateResult *result, SZ idx, MenuEntryType type) {
    Menu const *menu                    = result->menu;
    MenuProperties const *props         = &menu->props;
    Rectangle const *rec_text           = nullptr;
    Rectangle const *rec_box            = nullptr;
    MenuEntryStyle const *entry_style   = nullptr;
    MenuBackgroundStyle const *bg_style = nullptr;
    C8 const *text                      = nullptr;
    AFont *font                         = nullptr;
    auto override_text_color            = BLANK;
    BOOL text_color_override_enabled    = false;
    auto text_shadow_color              = BLACK;

    switch (type) {
        case MENU_ENTRY_TYPE_LABEL: {
            rec_text    = &result->label_text;
            rec_box     = &result->label_box;
            entry_style = &props->label_entry;
            bg_style    = &props->label_bg;
            font        = asset_get_font(menu->props.label_font.name->c, ui_font_size(menu->props.label_font.size_perc));
            text        = menu->label_get(menu->label_data, idx, &override_text_color, &text_color_override_enabled);
        } break;
        case MENU_ENTRY_TYPE_DESC: {
            rec_text    = &result->desc_text;
            rec_box     = &result->desc_box;
            entry_style = &props->desc_entry;
            bg_style    = &props->desc_bg;
            font        = asset_get_font(menu->props.desc_font.name->c, ui_font_size(menu->props.desc_font.size_perc));
            text        = menu->desc_get(menu->desc_data, idx, &override_text_color, &text_color_override_enabled);
        } break;
        case MENU_ENTRY_TYPE_VALUE: {
            // Value is optional.
            if (!menu->value_get) { return; }
            rec_text    = &result->value_text;
            rec_box     = &result->value_box;
            entry_style = &props->value_entry;
            bg_style    = &props->value_bg;
            font        = asset_get_font(menu->props.value_font.name->c, ui_font_size(menu->props.value_font.size_perc));
            text        = menu->value_get(menu->value_data, idx, &override_text_color, &text_color_override_enabled);
            if (!text) { return; }
        } break;
        default: {
            _unreachable_();
        }
    }

    Color entry_color = entry_style->normal;
    if (text_color_override_enabled) { entry_color = override_text_color; }

    BOOL const i_am_selected = idx == menu->selected_entry;
    BOOL const i_am_hovered  = idx == menu->hovered_entry;

    if (i_am_hovered) {
        entry_color = entry_style->hovered;
    } else if (i_am_selected) {
        entry_color = entry_style->selected;
    }
    Color const shadow_color = bg_style->shadow;
    Color const border_color = bg_style->border;
    Color const bg_color     = bg_style->normal;

    // Shadow.
    d2d_rectangle_rec(
        Rectangle{
            rec_box->x + ui_scale_x(props->shadow_size_perc),
            rec_box->y + ui_scale_x(props->shadow_size_perc),
            rec_box->width,
            rec_box->height,
        },
        shadow_color);

    // Border. (only if border_size > 0)
    if (ui_scale_x(props->border_size_perc) > 0.0F) {
        F32 const border_size = (F32)(S32)ui_scale_x(props->border_size_perc);
        d2d_rectangle_lines_ex(
            Rectangle{
                rec_box->x - border_size,
                rec_box->y - border_size,
                rec_box->width + (2 * border_size),
                rec_box->height + (2 * border_size),
            },
            border_size, border_color);
    }

    // Background. (only if bg_color.a > 0 that means it's not transparent and the text is not empty)
    if (bg_color.a > 0 && text) { d2d_rectangle_rec(*rec_box, bg_color); }

    // Text.
    d2d_text_shadow(font, text, Vector2{rec_text->x, rec_text->y}, entry_color, text_shadow_color, ui_shadow_offset(0.1F, 0.075F));
}

void menu_draw_2d_hud(Menu *menu) {
    if (!menu->enabled) { return; }

    for (SZ i = 0; i < menu->entry_count; i++) {
        MenuEntry *entry = &menu->entries[i];
        i_draw_entry(&entry->update_result, i, MENU_ENTRY_TYPE_DESC);
        i_draw_entry(&entry->update_result, i, MENU_ENTRY_TYPE_VALUE);
        i_draw_entry(&entry->update_result, i, MENU_ENTRY_TYPE_LABEL);

        // Draw selection line if this is the selected entry
        if (menu->props.horizontal_selection_indicator && i == menu->selected_entry) {
            MenuEntryUpdateResult const *result = &entry->update_result;
            // Calculate line position - place it under the description box
            F32 const line_y = result->desc_box.y + result->desc_box.height + 2.0F;
            // Draw the line using only description width
            d2d_rectangle_rec(Rectangle{result->label_box.x, line_y, result->desc_box.width, ui_scale_y(0.15F)}, menu->props.horizontal_selection_indicator_color);
        }
    }
}

void menu_draw_2d_dbg(Menu *menu) {
    if (!menu->enabled) { return; }
    if (!c_debug__menu_info) { return; }

    Vector2 const shadow_offset = {1.0F, 1.0F};
    auto shadow_color           = BLACK;
    auto label_color            = MAGENTA;
    auto desc_color             = CYAN;
    auto value_color            = YELLOW;

    for (SZ i = 0; i < menu->entry_count; i++) {
        MenuEntry *entry                 = &menu->entries[i];
        BOOL const is_selected           = i == menu->selected_entry;
        BOOL const is_hovered            = i == menu->hovered_entry;
        MenuEntryUpdateResult const *res = &entry->update_result;

        F32 fade_amount = 0.25F;
        fade_amount    += is_selected ? 0.35F : 0.0F;
        fade_amount    += is_hovered ? 0.35F : 0.0F;

        Vector2 const label_title_pos = {res->label_box.x + 2.0F, res->label_box.y + 2.0F};
        d2d_rectangle_rec(res->label_box, Fade(label_color, fade_amount));
        d2d_rectangle_lines_ex(res->label_box, 1.0F, label_color);
        d2d_text_shadow(menu->debug_font, "Label", label_title_pos, label_color, shadow_color, shadow_offset);

        Vector2 const desc_title_pos = {res->desc_box.x + 2.0F, res->desc_box.y + 2.0F};
        d2d_rectangle_rec(res->desc_box, Fade(desc_color, fade_amount));
        d2d_rectangle_lines_ex(res->desc_box, 1.0F, desc_color);
        d2d_text_shadow(menu->debug_font, "Description", desc_title_pos, desc_color, shadow_color, shadow_offset);

        if (menu->value_get) {
            Vector2 const value_title_pos = {res->value_box.x + 2.0F, res->value_box.y + 2.0F};
            d2d_rectangle_rec(res->value_box, Fade(value_color, fade_amount));
            d2d_rectangle_lines_ex(res->value_box, 1.0F, value_color);
            d2d_text_shadow(menu->debug_font, "Value", value_title_pos, value_color, shadow_color, shadow_offset);
        }
    }
}

void menu_extra_override_static_color(Color *color_ptr, BOOL *color_override_ptr, Color color) {
    *color_ptr          = color;
    *color_override_ptr = true;
}

void menu_extra_override_toggle_color(Color *color_ptr, BOOL *color_override_ptr, BOOL toggle) {
    *color_ptr          = toggle ? ONCOLOR : OFFCOLOR;
    *color_override_ptr = true;
}

void menu_extra_override_lerp_color(Color *color_ptr, BOOL *color_override_ptr, Color a, Color b, F32 t) {
    *color_ptr          = color_lerp(a, b, t);
    *color_override_ptr = true;
}

void menu_extra_toggle_data_cb_toggle(void *data) {
    auto *toggle_data    = (MenuExtraToggleData *)data;
    *toggle_data->toggle = !*toggle_data->toggle;
    C8 const *state      = *toggle_data->toggle ? "enabled" : "disabled";
    Color const color    = *toggle_data->toggle ? GREEN : RED;
    mi(TS("%s %s", toggle_data->name, state)->c, color);
}
