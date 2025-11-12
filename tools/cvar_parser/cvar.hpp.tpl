#pragma once

#include "common.hpp"

// WARN: DO NOT EDIT - THIS IS A GENERATED FILE!

#define CVAR_COUNT {{CVAR_COUNT}}
#define CVAR_FILE_NAME "ouro.cvar"
#define CVAR_NAME_MAX_LENGTH 128
#define CVAR_STR_MAX_LENGTH 128

#define CVAR_LONGEST_NAME_LENGTH {{CVAR_LONGEST_NAME_LENGTH}}
#define CVAR_LONGEST_VALUE_LENGTH {{CVAR_LONGEST_VALUE_LENGTH}}
#define CVAR_LONGEST_TYPE_LENGTH {{CVAR_LONGEST_TYPE_LENGTH}}

struct CVarStr {
    C8 cstr[CVAR_STR_MAX_LENGTH];
};

enum CVarType : U8 {
    CVAR_TYPE_BOOL,
    CVAR_TYPE_S32,
    CVAR_TYPE_F32,
    CVAR_TYPE_CVARSTR,
};

struct CVarMeta {
    C8 name[CVAR_NAME_MAX_LENGTH];
    void *address;
    CVarType type;
    C8 comment[CVAR_NAME_MAX_LENGTH];
};

{{CVAR_DECLARATIONS}}

extern const CVarMeta cvar_meta_table[CVAR_COUNT];

void cvar_load();
void cvar_save();
