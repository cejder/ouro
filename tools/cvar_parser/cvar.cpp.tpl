#include "{{OUTPUT_FILE_BASE}}.hpp"
#include "assert.hpp"
#include "log.hpp"
#include "memory.hpp"
#include "std.hpp"
#include "string.hpp"

#include <glm/common.hpp>
#include <raylib.h>

// WARN: DO NOT EDIT - THIS IS A GENERATED FILE!

{{CVAR_DEFINITIONS}}

CVarMeta const cvar_meta_table[CVAR_COUNT] = {
{{CVAR_META_TABLE}}
};

void cvar_save() {
    String *t = string_create_empty(MEMORY_TYPE_TARENA);

    // Calculate the maximum variable name length
    SZ max_var_name_length = 0;
    for (const auto &cvar : cvar_meta_table) {
        C8 const *sep = ou_strstr(cvar.name, "__");
        if (sep) {
            SZ const var_name_len = ou_strlen(sep + 2);
            max_var_name_length = glm::max(var_name_len, max_var_name_length);
        }
    }

    C8 current_header[CVAR_NAME_MAX_LENGTH] = "";

    for (const auto &cvar : cvar_meta_table) {
        // Split variable name
        C8 const *sep = ou_strstr(cvar.name, "__");
        if (!sep) {
            lle("cvar name %s missing 2x underscore separator", cvar.name);
            continue;
        }

        // Extract header name
        SZ const header_len = (SZ)(sep - cvar.name);
        C8 header[CVAR_NAME_MAX_LENGTH];
        ou_strncpy(header, cvar.name, header_len);
        header[header_len] = '\0';

        // Extract variable name and get its length
        C8 const *var_name = sep + 2;
        SZ const var_name_len = ou_strlen(var_name);

        // Write header if it changed
        if (ou_strcmp(current_header, header) != 0) {
            if (current_header[0] != '\0') {
                string_append(t, "\n"); // Add blank line between sections
            }
            string_appendf(t, "[%s]\n", header);
            ou_strcpy(current_header, header);
        }

        // Write variable name and padding
        string_appendf(t, "%s", var_name);
        string_appendf(t, "%*.s : ", (S32)(max_var_name_length - var_name_len), " ");

        switch (cvar.type) {
            case CVAR_TYPE_BOOL: {
                string_append(t, BOOL_TO_STR(*(BOOL *)(cvar.address)));
            } break;
            case CVAR_TYPE_S32: {
                string_appendf(t, "%d", *(S32 *)(cvar.address));
            } break;
            case CVAR_TYPE_F32: {
                string_appendf(t, "%.8f", *(F32 *)(cvar.address));
            } break;
            case CVAR_TYPE_CVARSTR: {
                string_appendf(t, "%s", ((CVarStr *)(cvar.address))->cstr);
            } break;
            default: {
                _unimplemented_();
            }
        }

        // Add comment if present
        if (cvar.comment[0] != '\0') {
            string_appendf(t, " # %s", cvar.comment);
        }
        string_append(t, "\n");
    }

    if (!SaveFileText(CVAR_FILE_NAME, t->c)) { lle("could not save cvars to %s", CVAR_FILE_NAME); }
}
