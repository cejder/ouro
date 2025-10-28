#pragma once

#include "common.hpp"

#include <fmod_common.h>
#include <stdio.h>
#include <atomic>

enum LLogLevel : U8 {
    LLOG_LEVEL_TRACE,
    LLOG_LEVEL_DEBUG,
    LLOG_LEVEL_INFO,
    LLOG_LEVEL_WARN,
    LLOG_LEVEL_ERROR,
    LLOG_LEVEL_FATAL,
    LLOG_LEVEL_NONE,
    LLOG_LEVEL_TTY,
    LLOG_LEVEL_COUNT,
};

enum LLogFlags : U8 {
    LLOG_FLAG_NONE = 0,
    LLOG_FLAG_EMACS = 1 << 0,
    LLOG_FLAG_SHORTFILE = 1 << 1,
    LLOG_FLAG_COLOR = 1 << 2,
    LLOG_FLAG_EXIT_ON_ERROR = 1 << 3,
};

struct LLogger {
    LLogLevel level;
    U32 flags;
    std::atomic_flag lock;
};

void llog_init();
void llog_set_level(LLogLevel level);
void llog_set_flags(U32 flags);
void llog_enable_flag(LLogFlags flag);
void llog_disable_flag(LLogFlags flag);
LLogLevel llog_get_level();
C8 const *llog_level_to_cstr(LLogLevel level);
void llog_format(LLogLevel level, C8 const *file, C8 const *func, U32 line, C8 const *fmt, ...) __attribute__((format(printf, 5, 6)));


// Third-party library callbacks
void llog_raylib_cb(S32 log_level, C8 const *text, va_list args) __attribute__((format(printf, 2, 0)));
void llog_mimalloc_cb(C8 const *message, void *arg);
FMOD_RESULT F_CALL llog_fmod_cb(FMOD_DEBUG_FLAGS flags, C8 const *file, S32 line, C8 const *func, C8 const *message);

#define lll(level, ...) llog_format (level,            __FILE__, __func__, __LINE__, __VA_ARGS__)
#define llt(...)        llog_format (LLOG_LEVEL_TRACE, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define lld(...)        llog_format (LLOG_LEVEL_DEBUG, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define lli(...)        llog_format (LLOG_LEVEL_INFO,  __FILE__, __func__, __LINE__, __VA_ARGS__)
#define llw(...)        llog_format (LLOG_LEVEL_WARN,  __FILE__, __func__, __LINE__, __VA_ARGS__)
#define lle(...)        llog_format (LLOG_LEVEL_ERROR, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define llf(...)        llog_format (LLOG_LEVEL_FATAL, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define lln(...)        llog_format (LLOG_LEVEL_NONE,  "",       "",       0,        __VA_ARGS__)
#define lltty(...)      llog_format (LLOG_LEVEL_TTY,   __FILE__, __func__, __LINE__, __VA_ARGS__)
#define llaaa           llog_format (LLOG_LEVEL_WARN,  __FILE__, __func__, __LINE__, "aaa")
#define llbbb           llog_format (LLOG_LEVEL_WARN,  __FILE__, __func__, __LINE__, "bbb")
#define lleee           llog_format (LLOG_LEVEL_FATAL, __FILE__, __func__, __LINE__, "eee")
