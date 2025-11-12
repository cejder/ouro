#pragma once

#include <stdio.h>  // IWYU pragma: keep
#include <stdlib.h>
#include "string.hpp"

#if OURO_IS_DEBUG_NUM
#define _assert_(expr, message)                                                                           \
    do {                                                                                                  \
        if (!(expr)) {                                                                                    \
            fprintf(stderr, "%s:%d:%s: ASSERTION VIOLATED: %s\n", __FILE__, __LINE__, __func__, message); \
            abort();                                                                                      \
        }                                                                                                 \
    } while (0)
#else
#define _assert_(expr, message) ((void)0)
#endif

#if OURO_IS_DEBUG_NUM
#define _assertf_(expr, fmt, ...)                                                               \
    do {                                                                                        \
        if (!(expr)) {                                                                          \
            fprintf(stderr, "%s:%d:%s: ASSERTION VIOLATED: %s\n", __FILE__, __LINE__, __func__, \
                    TS(fmt, __VA_ARGS__)->c);                                                   \
            abort();                                                                            \
        }                                                                                       \
    } while (0)
#else
#define _assertf_(expr, fmt, ...) ((void)0)
#endif

#define _unreachable_()   _assert_(false, "Unreachable code")
#define _fixme_(msg)      _assert_(false, msg)
#define _break_()         _assert_(false, "DEBUG BREAK")
#define _unimplemented_() _assert_(false, "Unimplemented code")
#define _todo_()          _assert_(false, "Todo code")
