#pragma once

#include "common.hpp"
#include "ring.hpp"

#define CON_IN_BUF_MAX 1024
#define CON_CMD_ARGS_MAX 5
#define CON_OUTPUT_BUFFER_CAPACITY 1000
#define CON_CMD_HISTORY_CAPACITY 1000

#define CONSOLE_FONT "GoMono"
#define CONSOLE_FONT_SIZE 22

#define CONSOLE_HISTORY_FILE_NAME "console_history.txt"

fwd_decl(ConCMD);
fwd_decl(AFont);
fwd_decl(ATexture);

RING_DECLARE(CstrRing, C8 const *);

struct Console {
    BOOL initialized;
    SZ vertical_history_offset;
    CstrRing cmd_history_buffer;
    SZ cmd_history_cursor;
    CstrRing output_buffer;
    C8 input_buffer[CON_IN_BUF_MAX];
    C8 prediction_buffer[CON_IN_BUF_MAX];
    SZ input_buffer_cursor;
    ATexture *scrollbar;
    SZ visible_line_count;
    BOOL was_enabled;
};

Console extern g_console;

void console_init();
void console_update();
void console_draw();
void console_do_parse_input();
void console_do_move_cursor(S32 offset);
void console_do_move_cursor_word(S32 direction);
void console_do_backspace();
void console_do_delete();
void console_do_history_previous();
void console_do_history_next();
void console_do_move_cursor_begin_line();
void console_do_move_cursor_end_line();
void console_do_kill_line();
void console_do_copy();
void console_do_paste();
void console_fill_prediction_buffer();
BOOL console_autocomplete_input_buffer();
void console_print_to_output(C8 const *message);
void console_draw_separator();
void console_clear();
