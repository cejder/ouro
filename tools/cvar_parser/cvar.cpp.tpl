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

void cvar_load() {
    // Try to load user config first, fall back to default
    U8 *content = LoadFileText(CVAR_FILE_NAME);
    BOOL from_default = false;

    if (!content) {
        // First run - try to load defaults
        content = LoadFileText("ouro.cvar.default");
        from_default = true;

        if (!content) {
            // No config file exists, use compiled-in defaults
            return;
        }
    }

    // Parse the INI file
    C8 current_section[CVAR_NAME_MAX_LENGTH] = "";
    C8 *line = (C8 *)content;
    C8 *end = line;

    while (*line) {
        // Find end of line
        while (*end && *end != '\n' && *end != '\r') {
            end++;
        }

        // Null-terminate the line
        C8 line_end_char = *end;
        if (*end) {
            *end = '\0';
        }

        // Trim leading whitespace
        while (*line == ' ' || *line == '\t') {
            line++;
        }

        // Skip empty lines and comments
        if (*line == '\0' || *line == '#') {
            goto next_line;
        }

        // Check for section header [section]
        if (*line == '[') {
            C8 *section_start = line + 1;
            C8 *section_end = section_start;
            while (*section_end && *section_end != ']') {
                section_end++;
            }
            if (*section_end == ']') {
                SZ section_len = section_end - section_start;
                ou_strncpy(current_section, section_start, section_len);
                current_section[section_len] = '\0';
            }
            goto next_line;
        }

        // Parse key : value
        if (current_section[0] != '\0') {
            C8 *key_start = line;
            C8 *sep = line;

            // Find the separator ':'
            while (*sep && *sep != ':') {
                sep++;
            }

            if (*sep == ':') {
                // Extract key name (trim trailing whitespace)
                C8 *key_end = sep - 1;
                while (key_end > key_start && (*key_end == ' ' || *key_end == '\t')) {
                    key_end--;
                }

                C8 key[CVAR_NAME_MAX_LENGTH];
                SZ key_len = (key_end - key_start) + 1;
                ou_strncpy(key, key_start, key_len);
                key[key_len] = '\0';

                // Extract value (skip leading whitespace after ':')
                C8 *value_start = sep + 1;
                while (*value_start == ' ' || *value_start == '\t') {
                    value_start++;
                }

                // Trim trailing whitespace and comments
                C8 *value_end = value_start;
                while (*value_end && *value_end != '#') {
                    value_end++;
                }
                value_end--;
                while (value_end > value_start && (*value_end == ' ' || *value_end == '\t')) {
                    value_end--;
                }

                C8 value[CVAR_STR_MAX_LENGTH];
                SZ value_len = (value_end - value_start) + 1;
                ou_strncpy(value, value_start, value_len);
                value[value_len] = '\0';

                // Build full cvar name: section__key
                C8 full_name[CVAR_NAME_MAX_LENGTH];
                ou_strcpy(full_name, current_section);
                ou_strcat(full_name, "__");
                ou_strcat(full_name, key);

                // Find and update the cvar
                for (const auto &cvar : cvar_meta_table) {
                    if (ou_strcmp(cvar.name, full_name) == 0) {
                        switch (cvar.type) {
                            case CVAR_TYPE_BOOL: {
                                if (ou_strcmp(value, "true") == 0) {
                                    *(BOOL *)(cvar.address) = true;
                                } else if (ou_strcmp(value, "false") == 0) {
                                    *(BOOL *)(cvar.address) = false;
                                }
                            } break;
                            case CVAR_TYPE_S32: {
                                *(S32 *)(cvar.address) = atoi(value);
                            } break;
                            case CVAR_TYPE_F32: {
                                *(F32 *)(cvar.address) = (F32)atof(value);
                            } break;
                            case CVAR_TYPE_CVARSTR: {
                                ou_strncpy(((CVarStr *)(cvar.address))->cstr, value, CVAR_STR_MAX_LENGTH - 1);
                                ((CVarStr *)(cvar.address))->cstr[CVAR_STR_MAX_LENGTH - 1] = '\0';
                            } break;
                        }
                        break;
                    }
                }
            }
        }

    next_line:
        // Move to next line
        if (line_end_char) {
            line = end + 1;
            // Skip '\r\n' or '\n\r'
            if (*line && (*line == '\n' || *line == '\r') && *line != line_end_char) {
                line++;
            }
            end = line;
        } else {
            break;
        }
    }

    UnloadFileText(content);

    // If we loaded from default, save it as user config for next time
    if (from_default) {
        cvar_save();
    }
}

void cvar_save() {
    String *t = string_create_empty(MEMORY_TYPE_ARENA_TRANSIENT);

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
