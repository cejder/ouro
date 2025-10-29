#include "arg.hpp"
#include "common.hpp"
#include "core.hpp"
#include "log.hpp"
#include "memory.hpp"
#include "unit.hpp"

#include <stdlib.h>

#ifdef _WIN32
__declspec(dllexport) U32 NvOptimusEnablement = 1;
__declspec(dllexport) S32 AmdPowerXpressRequestHighPerformance = 1;
#endif

S32 main(S32 argc, C8 const **argv) {
    args_add("JustTest",     "Execute tests and terminate the program",                "-t",  "--test",         ARG_TYPE_BOOL);
    args_add("InDebugger",   "Simplify log output for compatibility with GDB",         "-d",  "--debugger",     ARG_TYPE_BOOL);
    args_add("InEmacs",      "Adjust log format for Emacs compilation buffer parsing", "-e",  "--emacs",        ARG_TYPE_BOOL);
    args_add("RebuildBlob",  "Force recreation of the asset blob file",                "-r",  "--rebuild-blob", ARG_TYPE_BOOL);
    args_add("SteamDeck",    "Run with Steam Deck configuration",                      "-sd", "--steam-deck",   ARG_TYPE_BOOL);
    args_add("Help",         "Show this help message and exit",                        "-h",  "--help",         ARG_TYPE_BOOL);

    args_parse(OURO_TITLE, argc, argv);

    if (args_get_bool("Help")) {
        args_usage();
        return EXIT_SUCCESS;
    }

    llog_init();

    if (!args_get_bool("InDebugger")) {
        llog_enable_flag(LLOG_FLAG_COLOR);
        llog_enable_flag(LLOG_FLAG_SHORTFILE);
        if (args_get_bool("InEmacs")) { llog_enable_flag(LLOG_FLAG_EMACS); }
    }

    if (OURO_IS_DEBUG_NO_DEVEL) {
        args_print();
    }

#ifdef OURO_TRACE
    llog_set_level(LLOG_LEVEL_TRACE);
    C8 const *build_type = "TRACE";
#elif OURO_DEBUG
    llog_set_level(LLOG_LEVEL_DEBUG);
    C8 const *build_type = "DEBUG";
#elif OURO_DEVEL
    llog_set_level(args_get_bool("InEmacs") ? LLOG_LEVEL_WARN : LLOG_LEVEL_DEBUG);
    C8 const *build_type = "DEVEL";
#elif OURO_RELEASE
    llog_set_level(LLOG_LEVEL_INFO);
    C8 const *build_type = "RELEASE";
#else
    exit(EXIT_FAILURE);
#endif

    // If we are in debugger mode, overwrite the log level to error only.
    if (args_get_bool("InDebugger")) {
        llog_set_level(LLOG_LEVEL_ERROR);
        lld("Starting ouro %s build in debugger mode, if you are seeing this outside of a debugger, something is wrong", build_type);
    } else {
        lld("Starting ouro %s build", build_type);
    }

    MemorySetup const setup = {
        .memory_alignment         = 64,

        .permanent_arena_verbose  = false,
        .permanent_arena_capacity = MEBI(1024),

        .debug_arena_verbose      = false,
        .debug_arena_capacity     = MEBI(256),

        .transient_arena_verbose  = false,
        .transient_arena_capacity = MEBI(512),

        .math_arena_verbose       = false,
        .math_arena_capacity      = MEBI(64),
    };

    memory_init(setup);
    core_init(OURO_MAJOR, OURO_MINOR, OURO_PATCH, build_type);
    core_run();
    core_quit();
    memory_quit();

    exit(EXIT_SUCCESS);
}
