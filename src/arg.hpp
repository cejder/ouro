#pragma once

#include "common.hpp"

#define ARG_TYPE_STRING_MAX_LENGTH 128
#define ARG_MAX_COUNT 128
#define ARG_VALUE_BUFFER_SIZE 256

enum ArgType : U8 {
    ARG_TYPE_BOOL,
    ARG_TYPE_STRING,
    ARG_TYPE_INTEGER,
    ARG_TYPE_FLOAT,
};

struct Arg {
    C8 const *name;
    C8 const *description;
    C8 const *short_flag;
    C8 const *long_flag;

    ArgType type;
    union {
        BOOL as_bool;
        C8 as_string[ARG_TYPE_STRING_MAX_LENGTH];
        S32 as_integer;
        F32 as_float;
    };
};

struct Args {
    C8 const *project_name;
    Arg args[ARG_MAX_COUNT];
    SZ arg_count;
};

void args_parse(C8 const *project_name, S32 argc, C8 const **argv);
void args_add(C8 const *name, C8 const *description, C8 const *short_flag, C8 const *long_flag, ArgType type);
void args_print();
void args_usage();

Arg *args_get(C8 const *name);
BOOL args_get_bool(C8 const *name);
C8 const *args_get_string(C8 const *name);
S32 args_get_s32(C8 const *name);
F32 args_get_f32(C8 const *name);
