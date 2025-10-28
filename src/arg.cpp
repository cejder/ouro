#include "arg.hpp"
#include "assert.hpp"
#include "std.hpp"

#include <errno.h>
#include <glm/common.hpp>

#define PRINT(fmt, ...) ou_fprintf(stdout, fmt, __VA_ARGS__)
#define ERR(fmt, ...) ou_fprintf(stderr, fmt, __VA_ARGS__)

Args static i_args = {};

BOOL static i_parse(Arg *arg, C8 const *flag, C8 const *value) {
    if (ou_strcmp(flag, arg->short_flag) != 0 && ou_strcmp(flag, arg->long_flag) != 0) { return false; }

    switch (arg->type) {
        case ARG_TYPE_BOOL: {
            arg->as_bool = true;
        } break;

        case ARG_TYPE_STRING: {
            if (!value) {
                ERR("Error: Missing value for argument %s\n", flag);
                exit(EXIT_FAILURE);
            }
            ou_strncpy(arg->as_string, value, ARG_TYPE_STRING_MAX_LENGTH - 1);
            arg->as_string[ARG_TYPE_STRING_MAX_LENGTH - 1] = '\0';
        } break;

        case ARG_TYPE_INTEGER: {
            if (!value) {
                ERR("Error: Missing value for argument %s\n", flag);
                exit(EXIT_FAILURE);
            }
            C8 *endptr = nullptr;
            errno = 0;
            long const val = strtol(value, &endptr, 10);
            if (errno != 0 || *endptr != '\0' || val != (S32)val) {
                ERR("Error: Invalid INTEGER value '%s' for argument %s\n", value, flag);
                exit(EXIT_FAILURE);
            }
            arg->as_integer = (S32)val;
        } break;

        case ARG_TYPE_FLOAT: {
            if (!value) {
                ERR("Error: Missing value for argument %s\n", flag);
                exit(EXIT_FAILURE);
            }
            C8 *endptr = nullptr;
            errno = 0;
            F64 const val = ou_strtod_endptr(value, &endptr);
            if (errno != 0 || *endptr != '\0') {
                ERR("Error: Invalid FLOAT value '%s' for argument %s\n", value, flag);
                exit(EXIT_FAILURE);
            }
            arg->as_float = (F32)val;
        } break;

        default: {
            _unreachable_();
        }
    }

    return true;
}

BOOL static i_is_negative_number(C8 const *str) {
    if (!str || str[0] != '-') { return false; }
    if (str[1] == '\0') { return false; }  // Just a lone "-"

    // Check if everything after the '-' is digits (with optional decimal point)
    BOOL has_digit = false;
    BOOL has_decimal = false;
    for (SZ i = 1; str[i] != '\0'; i++) {
        if (str[i] == '.') {
            if (has_decimal) { return false; }  // Multiple decimal points
            has_decimal = true;
        } else if (str[i] >= '0' && str[i] <= '9') {
            has_digit = true;
        } else {
            return false;  // Invalid character
        }
    }
    return has_digit;  // Must have at least one digit
}

void args_parse(C8 const *project_name, S32 argc, C8 const **argv) {
    i_args.project_name = project_name;

    for (S32 i = 1; i < argc; i++) {
        C8 const *current_flag = argv[i];
        C8 const *next_value = (i + 1 < argc) ? argv[i + 1] : nullptr;
        BOOL found = false;

        for (SZ j = 0; j < i_args.arg_count; j++) {
            if (i_parse(&i_args.args[j], current_flag, next_value)) {
                // For non-boolean arguments, consume the next argument if it exists and either:
                // 1. Doesn't start with '-', OR
                // 2. Starts with '-' but is a negative number
                if (i_args.args[j].type != ARG_TYPE_BOOL && next_value && (next_value[0] != '-' || i_is_negative_number(next_value))) { i++; }
                found = true;
                break;
            }
        }

        if (!found) {
            ERR("Unknown option '%s'", current_flag);
            exit(EXIT_FAILURE);
        }
    }
}

void args_add(C8 const *name, C8 const *description, C8 const *short_flag, C8 const *long_flag, ArgType type) {
    if (i_args.arg_count >= ARG_MAX_COUNT) {
        ERR("Too many arguments (max %zu)", (SZ)ARG_MAX_COUNT);
        exit(EXIT_FAILURE);
    }

    Arg *arg         = &i_args.args[i_args.arg_count];
    arg->name        = name;
    arg->description = description;
    arg->short_flag  = short_flag;
    arg->long_flag   = long_flag;
    arg->type        = type;

    switch (type) {
        case ARG_TYPE_BOOL: {
            arg->as_bool = false;
        } break;
        case ARG_TYPE_STRING: {
            arg->as_string[0] = '\0';
        } break;
        case ARG_TYPE_INTEGER: {
            arg->as_integer = 0;
        } break;
        case ARG_TYPE_FLOAT: {
            arg->as_float = 0.0F;
        } break;
        default: {
            _unreachable_();
        }
    }

    i_args.arg_count++;
}

C8 static const *i_arg_value_to_string(Arg *arg) {
    _Thread_local static C8 buffer[ARG_VALUE_BUFFER_SIZE];

    switch (arg->type) {
        case ARG_TYPE_BOOL: {
            return BOOL_TO_STR(arg->as_bool);
        }

        case ARG_TYPE_STRING: {
            return arg->as_string[0] ? arg->as_string : "(empty)";
        }

        case ARG_TYPE_INTEGER: {
            ou_snprintf(buffer, ARG_VALUE_BUFFER_SIZE, "%d", arg->as_integer);
            return buffer;
        }

        case ARG_TYPE_FLOAT: {
            ou_snprintf(buffer, ARG_VALUE_BUFFER_SIZE, "%.2f", arg->as_float);
            return buffer;
        }

        default: {
            _unreachable_();
        }
    }
}

void args_print() {
    SZ max_name_len = 0;
    for (SZ i = 0; i < i_args.arg_count; i++) {
        SZ const len = ou_strlen(i_args.args[i].name);
        max_name_len = glm::max(len, max_name_len);
    }

    PRINT("%s\n", "Args:");
    for (SZ i = 0; i < i_args.arg_count; i++) { PRINT("   %*s: %s\n", (S32)max_name_len, i_args.args[i].name, i_arg_value_to_string(&i_args.args[i])); }
}

void args_usage() {
    SZ max_name_len  = 0;
    SZ max_short_len = 0;
    SZ max_long_len  = 0;

    for (SZ i = 0; i < i_args.arg_count; i++) {
        Arg *arg           = &i_args.args[i];
        SZ const name_len  = ou_strlen(arg->name);
        SZ const short_len = ou_strlen(arg->short_flag);
        SZ const long_len  = ou_strlen(arg->long_flag);

        max_name_len  = glm::max(name_len, max_name_len);
        max_short_len = glm::max(short_len, max_short_len);
        max_long_len  = glm::max(long_len, max_long_len);
    }

    PRINT("Usage: %s [options]\n", i_args.project_name);
    PRINT("%s\n", "Options:");

    for (SZ i = 0; i < i_args.arg_count; i++) {
        Arg *arg = &i_args.args[i];
        PRINT("   %*s:  %-*s  or  %-*s    %s\n", (S32)max_name_len, arg->name, (S32)max_short_len, arg->short_flag, (S32)max_long_len, arg->long_flag,
              arg->description);
    }
}

Arg *args_get(C8 const *name) {
    for (SZ i = 0; i < i_args.arg_count; i++) {
        if (ou_strcmp(i_args.args[i].name, name) == 0) { return &i_args.args[i]; }
    }
    return nullptr;
}

BOOL args_get_bool(C8 const *name) {
    Arg *arg = args_get(name);
    if (!arg) {
        ERR("Argument '%s' not found", name);
        return false;
    }
    if (arg->type != ARG_TYPE_BOOL) {
        ERR("Argument '%s' is not a BOOL", name);
        return false;
    }
    return arg->as_bool;
}

C8 const *args_get_string(C8 const *name) {
    Arg *arg = args_get(name);
    if (!arg) {
        ERR("Argument '%s' not found", name);
        return nullptr;
    }
    if (arg->type != ARG_TYPE_STRING) {
        ERR("Argument '%s' is not a STRING", name);
        return nullptr;
    }
    return arg->as_string;
}

S32 args_get_s32(C8 const *name) {
    Arg *arg = args_get(name);
    if (!arg) {
        ERR("Argument '%s' not found", name);
        return 0;
    }
    if (arg->type != ARG_TYPE_INTEGER) {
        ERR("Argument '%s' is not an INTEGER", name);
        return 0;
    }
    return arg->as_integer;
}

F32 args_get_f32(C8 const *name) {
    Arg *arg = args_get(name);
    if (!arg) {
        ERR("Argument '%s' not found", name);
        return 0.0F;
    }
    if (arg->type != ARG_TYPE_FLOAT) {
        ERR("Argument '%s' is not a FLOAT", name);
        return 0.0F;
    }
    return arg->as_float;
}
