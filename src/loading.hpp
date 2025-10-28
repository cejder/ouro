#pragma once

#include "common.hpp"

#include "unit.hpp"

struct Loading {
    SZ current;
    SZ max;
};

#define LD_TRACK(loading, label, ...)                                                          \
    do {                                                                                       \
        F32 const start_time_##__COUNTER__ = time_get_glfw();                                  \
        loading_progress_forward(loading, label);                                              \
        __VA_ARGS__;                                                                           \
        F32 const elapsed_##__COUNTER__ = time_get_glfw() - start_time_##__COUNTER__;          \
        String *label_str = TS("%s", label);                                                   \
        string_remove_ouc_codes(label_str);                                                    \
        lli("loading \"%s\" took %.2fms", label_str->c, BASE_TO_MILLI(elapsed_##__COUNTER__)); \
    } while (0)

void loading_progress_forward(Loading *loading, C8 const *label);
void loading_finish(Loading *loading);
void loading_prepare(Loading *loading, SZ max);
F32 loading_get_progress_perc(Loading *loading);
