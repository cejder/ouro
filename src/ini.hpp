#pragma once

#include "common.hpp"
#include "memory.hpp"

#define INI_MAX_ENTRIES 1024

fwd_decl(String);

struct INIEntry {
    String *header;
    String *key;
    String *value;
};

struct INIFile {
    MemoryType memory_type;
    String *path;
    String *content;
    INIEntry entries[INI_MAX_ENTRIES];
    SZ entry_count;
};

INIFile *ini_file_create(MemoryType memory_type, C8 const *path, BOOL *exists);
void ini_file_parse(INIFile *ini);
BOOL ini_file_exists(INIFile *ini);
void ini_file_set_c8(INIFile *ini, C8 const *header, C8 const *key, C8 const *value);
void ini_file_set_s32(INIFile *ini, C8 const *header, C8 const *key, S32 value);
void ini_file_set_f32(INIFile *ini, C8 const *header, C8 const *key, F32 value);
void ini_file_set_b8(INIFile *ini, C8 const *header, C8 const *key, BOOL value);
C8 const *ini_file_get_c8(INIFile *ini, C8 const *header, C8 const *key);
S32 ini_file_get_s32(INIFile *ini, C8 const *header, C8 const *key, S32 default_value);
F32 ini_file_get_f32(INIFile *ini, C8 const *header, C8 const *key, F32 default_value);
BOOL ini_file_get_b8(INIFile *ini, C8 const *header, C8 const *key, BOOL default_value);
BOOL ini_file_header_exists(INIFile *ini, C8 const *header);
void ini_file_save(INIFile *ini);
