#include "message.hpp"
#include "assert.hpp"
#include "asset.hpp"
#include "cvar.hpp"
#include "ease.hpp"
#include "math.hpp"
#include "render.hpp"
#include "std.hpp"

Messages static i_messages = {};

Color static const i_type_to_colors[MESSAGE_TYPE_COUNT] = {GREEN, YELLOW, RED};

void static i_set_text(Message *message, C8 const *text) {
    ou_strncpy(message->text, text, MESSAGE_MAX_LENGTH);
}

void static i_add(Message **message, MessageType type, C8 const *text, F32 duration) {
    _assert_(ou_strlen(text) < MESSAGE_MAX_LENGTH, "Message has too many characters");

    for (auto &entry : i_messages.messages) {
        if (!entry.alarm.is_active) {
            *message = &entry;
            break;
        }
    }

    // Check if *message is null (i.e., no inactive messages found)
    if (*message == nullptr) {
        *message = &i_messages.messages[0];
        for (SZ i = 1; i < MESSAGE_MAX; ++i) {
            if (alarm_get_remaining(&i_messages.messages[i].alarm) < alarm_get_remaining(&(*message)->alarm)) { *message = &i_messages.messages[i]; }
        }
        i_set_text(*message, "");
        alarm_stop(&(*message)->alarm);
    }

    // Set the message properties
    i_set_text(*message, text);
    alarm_start(&(*message)->alarm, duration, false);
    (*message)->type       = type;
    (*message)->spawn_time = 0.0F;
}

void messages_add(MessageType type, C8 const *text, Color text_color, F32 duration) {
    Message *message = nullptr;
    i_add(&message, type, text, duration);
    message->text_color = text_color;
    message->is_ouc     = false;
}

void messages_add_ouc(MessageType type, C8 const *text, Color text_color, F32 duration) {
    Message *message = nullptr;
    i_add(&message, type, text, duration);
    message->text_color = text_color;
    message->is_ouc     = true;
}

void messages_vprintf(MessageType type, Color text_color, F32 duration, C8 const *fmt, va_list args) {
    C8 formatted_text[MESSAGE_MAX_LENGTH];
    ou_vsnprintf(formatted_text, sizeof(formatted_text), fmt, args);
    messages_add(type, formatted_text, text_color, duration);
}

void messages_printf(MessageType type, Color text_color, F32 duration, C8 const *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    messages_vprintf(type, text_color, duration, fmt, args);
    va_end(args);
}

void messages_ouc_vprintf(MessageType type, Color text_color, F32 duration, C8 const *fmt, va_list args) {
    C8 formatted_text[MESSAGE_MAX_LENGTH];
    ou_vsnprintf(formatted_text, sizeof(formatted_text), fmt, args);
    messages_add_ouc(type, formatted_text, text_color, duration);
}

void messages_ouc_printf(MessageType type, Color text_color, F32 duration, C8 const *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    messages_ouc_vprintf(type, text_color, duration, fmt, args);
    va_end(args);
}

void messages_update(F32 dtu) {
    for (auto &entry : i_messages.messages) {
        if (!entry.alarm.is_active) { continue; }

        entry.spawn_time += dtu;

        if (alarm_tick(&entry.alarm, dtu)) {
            i_set_text(&entry, "");
            alarm_stop(&entry.alarm);
        }
    }
}

struct DrawableMessage {
    Message *message;
    F32 elapsed_time;
};

S32 static i_compare_messages(void const *a, void const *b) {
    auto const *msg_a = (DrawableMessage const *)a;
    auto const *msg_b = (DrawableMessage const *)b;

    F32 const remaining_a = alarm_get_remaining(&msg_a->message->alarm);
    F32 const remaining_b = alarm_get_remaining(&msg_b->message->alarm);

    if (remaining_a < remaining_b) { return -1; }
    if (remaining_a > remaining_b) { return  1; }

    return 0;
}

void messages_draw() {
    Vector2 const res = {(F32)c_video__render_resolution_width, (F32)c_video__render_resolution_height};
    S32 const font_size = ui_font_size(1.5F);
    AFont *font = asset_get_font("GoMono", font_size);

    // Load icon textures
    ATexture *icon_info  = asset_get_texture("message_info.png");
    ATexture *icon_warn  = asset_get_texture("message_warn.png");
    ATexture *icon_error = asset_get_texture("message_error.png");

    // Collect active messages
    DrawableMessage drawable_messages[MESSAGE_MAX] = {};
    SZ drawable_count = 0;
    for (auto &message : i_messages.messages) {
        if (message.alarm.is_active) {
            drawable_messages[drawable_count].message      = &message;
            drawable_messages[drawable_count].elapsed_time = message.alarm.elapsed;
            drawable_count++;
        }
    }
    if (drawable_count == 0) { return; }

    qsort(drawable_messages, drawable_count, sizeof(DrawableMessage), i_compare_messages);

    // Layout
    F32 const padding          = ui_scale_y(0.6F);
    F32 const icon_size        = ui_scale_y(1.6F);
    F32 const fade_in_duration = 0.2F;
    F32 const fade_out_start   = 0.85F;

    // Calculate total height
    F32 total_height = 0.0F;
    for (SZ i = 0; i < drawable_count; ++i) {
        Message *message       = drawable_messages[i].message;
        Vector2 const text_dim = message->is_ouc ? measure_text_ouc(font, message->text) : measure_text(font, message->text);
        total_height          += glm::max(text_dim.y, icon_size) + (padding * 2.0F) + padding;
    }

    F32 y_offset = res.y - total_height - padding;

    // Draw each message
    for (SZ i = 0; i < drawable_count; ++i) {
        Message *message         = drawable_messages[i].message;
        F32 const alarm_progress = alarm_get_progress_perc(&message->alarm);
        F32 const spawn_progress = glm::clamp(message->spawn_time / fade_in_duration, 0.0F, 1.0F);

        // Alpha
        F32 alpha_norm = 1.0F;
        if (spawn_progress < 1.0F) {
            alpha_norm = ease_out_cubic(spawn_progress, 0.0F, 1.0F, 1.0F);
        }
        if (alarm_progress > fade_out_start) {
            F32 const fade_progress = (alarm_progress - fade_out_start) / (1.0F - fade_out_start);
            alpha_norm             *= 1.0F - ease_in_quad(fade_progress, 0.0F, 1.0F, 1.0F);
        }
        U8 const alpha = (U8)(255.0F * alpha_norm);

        // Icon animation
        F32 const anim_duration = 0.6F;
        F32 icon_scale = 1.0F;
        F32 icon_rotation = 0.0F;
        if (message->spawn_time < anim_duration) {
            F32 const anim_t = message->spawn_time / anim_duration;
            icon_scale       = ease_out_elastic(anim_t, 0.0F, 1.0F, 1.0F);
            icon_rotation    = math_sin_f32(anim_t * 12.0F) * 8.0F * (1.0F - anim_t);
        }

        // Type data
        ATexture *icon     = icon_info;
        Color accent_color = {140, 180, 220, alpha};
        Color text_color   = {200, 210, 220, alpha};

        if (message->type == MESSAGE_TYPE_WARN) {
            icon         = icon_warn;
            accent_color = {220, 180, 120, alpha};
            text_color   = {220, 210, 190, alpha};
        } else if (message->type == MESSAGE_TYPE_ERROR) {
            icon         = icon_error;
            accent_color = {220, 120, 120, alpha};
            text_color   = {220, 200, 200, alpha};
        }

        // Dimensions
        Vector2 const text_dim = message->is_ouc ? measure_text_ouc(font, message->text) : measure_text(font, message->text);
        F32 const msg_height   = glm::max(text_dim.y, icon_size) + (padding * 2.0F);
        F32 const msg_width    = icon_size + (padding * 3.0F) + text_dim.x + padding;

        Rectangle const bg_rec = {
            (res.x - msg_width) / 2.0F,
            y_offset,
            msg_width,
            msg_height
        };

        // Background
        d2d_rectangle_rec(bg_rec, {16, 18, 22, alpha});

        // Icon
        Vector2 const icon_pos = {
            bg_rec.x + padding + (icon_size / 2.0F),
            bg_rec.y + (msg_height / 2.0F)
        };

        Rectangle const icon_src = {0, 0, (F32)icon->base.width, (F32)icon->base.height};
        Rectangle const icon_dst = {icon_pos.x, icon_pos.y, icon_size * icon_scale, icon_size * icon_scale};
        d2d_texture_pro(icon, icon_src, icon_dst, {icon_dst.width / 2.0F, icon_dst.height / 2.0F}, icon_rotation, accent_color);

        // Text
        Vector2 const text_pos = {
            bg_rec.x + padding + icon_size + padding,
            bg_rec.y + ((msg_height - text_dim.y) / 2.0F)
        };

        if (message->is_ouc) {
            d2d_text_ouc(font, message->text, text_pos, text_color);
        } else {
            d2d_text(font, message->text, text_pos, text_color);
        }

        // Border
        d2d_rectangle_lines((S32)bg_rec.x, (S32)bg_rec.y, (S32)bg_rec.width, (S32)bg_rec.height, accent_color);

        y_offset += msg_height + padding;
    }
}
