#pragma once

#include "common.hpp"
#include "info.hpp"
#include "loading.hpp"

struct Core {
    BOOL initialized;
    BOOL running;
    VersionInfo version_info;
    SZ cpu_core_count;
    Loading loading;
};

Core extern g_core;

void core_init(U8 major, U8 minor, U8 patch, C8 const *build_type);
void core_quit();
void core_error_quit();
void core_run();
void core_loop();
void core_update();
void core_draw();
void core_post();
