#pragma once

#include "common.hpp"

#define OURO_MAJOR 0
#define OURO_MINOR 3
#define OURO_PATCH 0
#define VERSION_INFO_CSTR_BUFFER_SIZE 512

struct VersionInfo {
    U8 major;
    U8 minor;
    U8 patch;
    C8 const *c;
    C8 const *build_type;
};

VersionInfo info_create_version_info(U8 major, U8 minor, U8 patch, C8 const *build_type);
SZ info_get_cpu_core_count();
void info_print_system(VersionInfo *info);
