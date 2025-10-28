#pragma once

#include "alarm.hpp"
#include "common.hpp"

#include <raylib.h>

#define MESSAGE_MAX 100
#define MESSAGE_MAX_LENGTH 256
#define MESSAGE_DEFAULT_DURATION 3.0F

fwd_decl(AFont);

enum MessageType : U8 {
    MESSAGE_TYPE_INFO,
    MESSAGE_TYPE_WARN,
    MESSAGE_TYPE_ERROR,
    MESSAGE_TYPE_COUNT,
};

struct Message {
    MessageType type;
    C8 text[MESSAGE_MAX_LENGTH];
    Alarm alarm;
    Color text_color;
    BOOL is_ouc;
    F32 spawn_time;  // Track when message was spawned for animations
};

struct Messages {
    Message messages[MESSAGE_MAX];
};

void messages_add(MessageType type, C8 const *text, Color text_color, F32 duration);
void messages_add_ouc(MessageType type, C8 const *text, Color text_color, F32 duration);
void messages_vprintf(MessageType type, Color text_color, F32 duration, C8 const *fmt, va_list args) __attribute__((format(printf, 4, 0)));
void messages_printf(MessageType type, Color text_color, F32 duration, C8 const *fmt, ...) __attribute__((format(printf, 4, 5)));
void messages_ouc_vprintf(MessageType type, Color text_color, F32 duration, C8 const *fmt, va_list args) __attribute__((format(printf, 4, 0)));
void messages_ouc_printf(MessageType type, Color text_color, F32 duration, C8 const *fmt, ...) __attribute__((format(printf, 4, 5)));
void messages_update(F32 dtu);
void messages_draw();

#define mq(text) messages_add(MESSAGE_TYPE_INFO, text, WHITE, MESSAGE_DEFAULT_DURATION)
#define mi(text, color) messages_add(MESSAGE_TYPE_INFO, text, color, MESSAGE_DEFAULT_DURATION)
#define mw(text, color) messages_add(MESSAGE_TYPE_WARN, text, color, MESSAGE_DEFAULT_DURATION)
#define me(text, color) messages_add(MESSAGE_TYPE_ERROR, text, color, MESSAGE_DEFAULT_DURATION)

#define mqo(text) messages_add_ouc(MESSAGE_TYPE_INFO, text, WHITE, MESSAGE_DEFAULT_DURATION)
#define mio(text, color) messages_add_ouc(MESSAGE_TYPE_INFO, text, color, MESSAGE_DEFAULT_DURATION)
#define mwo(text, color) messages_add_ouc(MESSAGE_TYPE_WARN, text, color, MESSAGE_DEFAULT_DURATION)
#define meo(text, color) messages_add_ouc(MESSAGE_TYPE_ERROR, text, color, MESSAGE_DEFAULT_DURATION)

#define mqd(text, duration) messages_add(MESSAGE_TYPE_INFO, text, WHITE, duration)
#define mid(text, color, duration) messages_add(MESSAGE_TYPE_INFO, text, color, duration)
#define mwd(text, color, duration) messages_add(MESSAGE_TYPE_WARN, text, color, duration)
#define med(text, color, duration) messages_add(MESSAGE_TYPE_ERROR, text, color, duration)

#define mqod(text, duration) messages_add_ouc(MESSAGE_TYPE_INFO, text, WHITE, duration)
#define miod(text, color, duration) messages_add_ouc(MESSAGE_TYPE_INFO, text, color, duration)
#define mwod(text, color, duration) messages_add_ouc(MESSAGE_TYPE_WARN, text, color, duration)
#define meod(text, color, duration) messages_add_ouc(MESSAGE_TYPE_ERROR, text, color, duration)

#define mqf(...) messages_printf(MESSAGE_TYPE_INFO, WHITE, MESSAGE_DEFAULT_DURATION, __VA_ARGS__)
#define mif(color, ...) messages_printf(MESSAGE_TYPE_INFO, color, MESSAGE_DEFAULT_DURATION, __VA_ARGS__)
#define mwf(color, ...) messages_printf(MESSAGE_TYPE_WARN, color, MESSAGE_DEFAULT_DURATION, __VA_ARGS__)
#define mef(color, ...) messages_printf(MESSAGE_TYPE_ERROR, color, MESSAGE_DEFAULT_DURATION, __VA_ARGS__)

#define mqof(...) messages_ouc_printf(MESSAGE_TYPE_INFO, WHITE, MESSAGE_DEFAULT_DURATION, __VA_ARGS__)
#define miof(color, ...) messages_ouc_printf(MESSAGE_TYPE_INFO, color, MESSAGE_DEFAULT_DURATION, __VA_ARGS__)
#define mwof(color, ...) messages_ouc_printf(MESSAGE_TYPE_WARN, color, MESSAGE_DEFAULT_DURATION, __VA_ARGS__)
#define meof(color, ...) messages_ouc_printf(MESSAGE_TYPE_ERROR, color, MESSAGE_DEFAULT_DURATION, __VA_ARGS__)

#define mqdf(duration, ...) messages_printf(MESSAGE_TYPE_INFO, WHITE, duration, __VA_ARGS__)
#define midf(color, duration, ...) messages_printf(MESSAGE_TYPE_INFO, color, duration, __VA_ARGS__)
#define mwdf(color, duration, ...) messages_printf(MESSAGE_TYPE_WARN, color, duration, __VA_ARGS__)
#define medf(color, duration, ...) messages_printf(MESSAGE_TYPE_ERROR, color, duration, __VA_ARGS__)

#define mqodf(duration, ...) messages_ouc_printf(MESSAGE_TYPE_INFO, WHITE, duration, __VA_ARGS__)
#define miodf(color, duration, ...) messages_ouc_printf(MESSAGE_TYPE_INFO, color, duration, __VA_ARGS__)
#define mwodf(color, duration, ...) messages_ouc_printf(MESSAGE_TYPE_WARN, color, duration, __VA_ARGS__)
#define meodf(color, duration, ...) messages_ouc_printf(MESSAGE_TYPE_ERROR, color, duration, __VA_ARGS__)
