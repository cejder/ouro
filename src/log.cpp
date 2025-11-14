#include "log.hpp"
#include "assert.hpp"
#include "common.hpp"
#include "console.hpp"
#include "core.hpp"
#include "std.hpp"

LLogger static i_logger = {};

C8 static const *i_level_str[LLOG_LEVEL_COUNT] = {
    "TRC", "DBG", "INF", "WRN", "ERR", "FAT", "NON", "TTY",
};

C8 static const *i_color_codes[LLOG_LEVEL_COUNT] = {
    "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m", "", "\x1b[37m",
};

C8 static const *i_get_short_file_name(C8 const *filepath) {
    C8 const *last_slash = ou_strrchr(filepath, '/');
    if (last_slash) { return last_slash + 1; }

    C8 const *last_backslash = ou_strrchr(filepath, '\\');
    if (last_backslash) { return last_backslash + 1; }

    return filepath;
}

void llog_init() {
    llog_enable_flag(LLOG_FLAG_EXIT_ON_ERROR);
    llog_enable_flag(LLOG_FLAG_QUIET_THIRD_PARTY);
}

void llog_set_level(LLogLevel level) {
    i_logger.level = level;
}

void llog_set_flags(U32 flags) {
    i_logger.flags = flags;
}

void llog_enable_flag(LLogFlags flag) {
    FLAG_SET(i_logger.flags, flag);
}

void llog_disable_flag(LLogFlags flag) {
    FLAG_CLEAR(i_logger.flags, (U32)flag);
}

LLogLevel llog_get_level() {
    return i_logger.level;
}

C8 const *llog_level_to_cstr(LLogLevel level) {
    if (level >= LLOG_LEVEL_COUNT) { return "UNK"; }
    return i_level_str[level];
}

void llog_format(LLogLevel level, C8 const *file, C8 const *func, U32 line, C8 const *fmt, ...) {
    if (level < i_logger.level && level != LLOG_LEVEL_NONE) { return; }

    while (i_logger.lock.test_and_set()) {}

    C8 const *display_file = file;
    if (FLAG_HAS(i_logger.flags, LLOG_FLAG_SHORTFILE)) { display_file = i_get_short_file_name(file); }

    BOOL const use_color = FLAG_HAS(i_logger.flags, LLOG_FLAG_COLOR);
    BOOL const in_emacs  = FLAG_HAS(i_logger.flags, LLOG_FLAG_EMACS);

    // Skip buffer output for NONE level
    if (level != LLOG_LEVEL_NONE) {
        // Timestamp for file output
        struct timespec ts_spec;
        clock_gettime(CLOCK_REALTIME, &ts_spec);
        struct tm *tm_info = localtime(&ts_spec.tv_sec);
        C8 ts[20];
        SZ const len = strftime(ts, sizeof(ts) - 4, "%H:%M:%S", tm_info);
        ou_snprintf(ts + len, sizeof(ts) - len, ".%03ld", ts_spec.tv_nsec / 1000000);

        // File buffer output (stdout)
        if (in_emacs) {
            if (use_color) { ou_fprintf(stdout, "%s", i_color_codes[level]); }
            ou_fprintf(stdout, "%s:%d: ", file, line);
            if (use_color) { ou_fprintf(stdout, "\x1b[0m"); }
        } else {
            if (use_color) {
                ou_fprintf(stdout, "\x1b[90m[%s]\x1b[0m %s[%s] %s:%s:%d: ", ts, i_color_codes[level], i_level_str[level], display_file, func, line);
            } else {
                ou_fprintf(stdout, "[%s] [%s] %s:%s:%d: ", ts, i_level_str[level], display_file, func, line);
            }
        }

        va_list args;
        va_start(args, fmt);
        ou_vfprintf(stdout, fmt, args);
        va_end(args);

        if (use_color && !in_emacs) { ou_fprintf(stdout, "\x1b[0m"); }
        ou_fprintf(stdout, "\n");
        ou_fflush(stdout);
    }

    // Console output (for in-game console)
    if (level != LLOG_LEVEL_TTY) {
        C8 console_msg[1024];
        SZ msg_len = 0;

        if (level != LLOG_LEVEL_NONE) { msg_len = (SZ)ou_snprintf(console_msg, sizeof(console_msg), "%s: |", i_level_str[level]); }

        if (msg_len < sizeof(console_msg)) {
            va_list args;
            va_start(args, fmt);
            ou_vsnprintf(console_msg + msg_len, sizeof(console_msg) - msg_len, fmt, args);
            va_end(args);
        } else {
            ou_strncpy(console_msg + msg_len, "Message too big to display.", sizeof(console_msg) - msg_len);
        }

        console_print_to_output(console_msg);
    }

    i_logger.lock.clear();

    // Exit on error if flag is set
    if ((level == LLOG_LEVEL_ERROR || level == LLOG_LEVEL_FATAL) && FLAG_HAS(i_logger.flags, LLOG_FLAG_EXIT_ON_ERROR)) {
        _break_();
        core_error_quit();
    }
}

void llog_raylib_cb(S32 log_level, C8 const *text, va_list args) {
    if (log_level >= LLOG_LEVEL_COUNT) { return; }

    LLogLevel llog_level = (LLogLevel)(log_level - 1);
    if (FLAG_HAS(i_logger.flags, LLOG_FLAG_QUIET_THIRD_PARTY)) { llog_level = LLOG_LEVEL_TRACE; }

    C8 formatted_msg[1024];
    ou_vsnprintf(formatted_msg, sizeof(formatted_msg), text, args);
    llog_format(llog_level, "raylib", "callback", 1, "%s", formatted_msg);
}

FMOD_RESULT F_CALL llog_fmod_cb(FMOD_DEBUG_FLAGS flags, C8 const *file, S32 line, C8 const *func, C8 const *message) {
    C8 modified_message[256];
    ou_strncpy(modified_message, message, sizeof(modified_message));
    modified_message[sizeof(modified_message) - 1] = '\0';

    // Remove newline character if it exists.
    SZ const msg_len = ou_strlen(modified_message);
    if (msg_len > 0 && modified_message[msg_len - 1] == '\n') { modified_message[msg_len - 1] = '\0'; }

    LLogLevel llog_level = LLOG_LEVEL_NONE;
    switch (flags) {
        case FMOD_DEBUG_LEVEL_ERROR: {
            llog_level = LLOG_LEVEL_ERROR;
        } break;
        case FMOD_DEBUG_LEVEL_WARNING: {
            llog_level = LLOG_LEVEL_WARN;
        } break;
        case FMOD_DEBUG_LEVEL_LOG:
        default: {
            llog_level = LLOG_LEVEL_INFO;
        } break;
    }

    if (FLAG_HAS(i_logger.flags, LLOG_FLAG_QUIET_THIRD_PARTY)) { llog_level = LLOG_LEVEL_TRACE; }

    llog_format(llog_level, file ? file : "fmod", func ? func : "callback", line > 0 ? (U32)line : 1, "%s", modified_message);
    return FMOD_OK;
}
