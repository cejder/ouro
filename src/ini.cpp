#include "ini.hpp"
#include "log.hpp"
#include "std.hpp"
#include "string.hpp"

#include <raylib.h>
#include <stdlib.h>

S32 static i_entry_compare(void const *a, void const *b) {
    auto const *entry_a = (INIEntry const *)a;
    auto const *entry_b = (INIEntry const *)b;

    S32 const header_cmp = ou_strcmp(entry_a->header->c, entry_b->header->c);
    if (header_cmp != 0) { return header_cmp; }

    return ou_strcmp(entry_a->key->c, entry_b->key->c);
}

INIFile *ini_file_create(MemoryType memory_type, C8 const *path, BOOL *exists) {
    auto *ini = mm(INIFile *, sizeof(INIFile), memory_type);
    if (!ini) {
        lle("Could not allocate memory for INI file (%s)", path);
        return nullptr;
    }

    ini->path        = string_create(memory_type, "%s", path);
    ini->memory_type = memory_type;
    ini->entry_count = 0;

    if (ini_file_exists(ini)) {
        if (exists) {  // This is optional so we only set it if it's not null.
            *exists = true;
        }
        ini_file_parse(ini);
    }

    return ini;
}

void ini_file_parse(INIFile *ini) {
    C8 *s = LoadFileText(ini->path->c);
    if (!s) {
        llw("We found the INI file (%s) but it seems fucky. We will act like we did not find it.", ini->path->c);
        return;
    }

    ini->content = string_create(ini->memory_type, "%s", s);
    UnloadFileText(s);

    SZ part_count          = 0;
    String *current_header = string_create_empty(ini->memory_type);
    String **parts         = string_split(ini->content, '\n', &part_count);

    for (SZ i = 0; i < part_count; ++i) {
        String *part        = parts[i];
        C8 const *part_cstr = part->c;

        // Skip comments.
        if (part_cstr[0] == ';') { continue; }

        // Skip empty lines.
        string_trim_space(part);
        if (part->length == 0) { continue; }

        if (part_cstr[0] == '[') {
            // Trim away the brackets.
            string_trim_front(part, "[");
            string_trim_back(part, "]");
            string_set(current_header, part->c);
        } else {
            // Split by "=" and trim space on both parts.
            SZ entry_part_count = 0;
            String **entry_parts = string_split(part, '=', &entry_part_count);
            if (entry_part_count != 2) {
                lle("INI file entry should contain 2 parts (e.g. \"key = value\") but we got \"%s\".", part->c);
                return;
            }

            string_trim_space(entry_parts[0]);
            string_trim_space(entry_parts[1]);

            INIEntry *entry = &ini->entries[ini->entry_count++];
            entry->header   = string_copy(current_header);
            entry->key      = entry_parts[0];
            entry->value    = entry_parts[1];
        }
    }
}

BOOL ini_file_exists(INIFile *ini) {
    return FileExists(ini->path->c);
}

void ini_file_set_c8(INIFile *ini, C8 const *header, C8 const *key, C8 const *value) {
    // Check if the entry already exists
    for (SZ i = 0; i < ini->entry_count; ++i) {
        INIEntry *entry = &ini->entries[i];
        if (string_equals_cstr(entry->header, header) && string_equals_cstr(entry->key, key)) {
            // Update the value
            string_set(entry->value, value);
            return;
        }
    }

    // If the entry does not exist, create a new one
    if (ini->entry_count < INI_MAX_ENTRIES) {
        INIEntry *new_entry = &ini->entries[ini->entry_count++];
        new_entry->header   = string_create(ini->path->memory_type, "%s", header);
        new_entry->key      = string_create(ini->path->memory_type, "%s", key);
        new_entry->value    = string_create(ini->path->memory_type, "%s", value);
    } else {
        lle("INI file has reached the maximum number of entries");
        return;
    }
}

void ini_file_set_s32(INIFile *ini, C8 const *header, C8 const *key, S32 value) {
    C8 buffer[32];
    ou_snprintf(buffer, sizeof(buffer), "%d", value);
    ini_file_set_c8(ini, header, key, buffer);
}

void ini_file_set_f32(INIFile *ini, C8 const *header, C8 const *key, F32 value) {
    C8 buffer[32];
    ou_snprintf(buffer, sizeof(buffer), "%.2f", value);
    ini_file_set_c8(ini, header, key, buffer);
}

void ini_file_set_b8(INIFile *ini, C8 const *header, C8 const *key, BOOL value) {
    ini_file_set_c8(ini, header, key, BOOL_TO_STR(value));
}

C8 const *ini_file_get_c8(INIFile *ini, C8 const *header, C8 const *key) {
    for (SZ i = 0; i < ini->entry_count; ++i) {
        INIEntry *entry = &ini->entries[i];
        if (string_equals_cstr(entry->header, header) && string_equals_cstr(entry->key, key)) { return entry->value->c; }
    }

    return nullptr;
}

S32 ini_file_get_s32(INIFile *ini, C8 const *header, C8 const *key, S32 default_value) {
    C8 const *value_str = ini_file_get_c8(ini, header, key);
    if (value_str) { return ou_atoi(value_str); }
    return default_value;
}

F32 ini_file_get_f32(INIFile *ini, C8 const *header, C8 const *key, F32 default_value) {
    C8 const *value_str = ini_file_get_c8(ini, header, key);
    if (value_str) { return ou_strtof(value_str); }
    return default_value;
}

BOOL ini_file_get_b8(INIFile *ini, C8 const *header, C8 const *key, BOOL default_value) {
    C8 const *value_str = ini_file_get_c8(ini, header, key);
    if (value_str) {
        if (ou_strcmp(value_str, "true")  == 0 || ou_strcmp(value_str, "1") == 0) { return true;  }
        if (ou_strcmp(value_str, "false") == 0 || ou_strcmp(value_str, "0") == 0) { return false; }
    }
    return default_value;
}

BOOL ini_file_header_exists(INIFile *ini, C8 const *header) {
    for (SZ i = 0; i < ini->entry_count; ++i) {
        INIEntry const *entry = &ini->entries[i];
        if (ou_strcmp(entry->header->c, header) == 0) { return true; }
    }

    return false;
}

void ini_file_save(INIFile *ini) {
    qsort(ini->entries, ini->entry_count, sizeof(INIEntry), i_entry_compare);

    String *output = string_create_empty(ini->path->memory_type);
    String *current_header = string_create_empty(ini->path->memory_type);

    for (SZ i = 0; i < ini->entry_count; ++i) {
        INIEntry const *entry = &ini->entries[i];

        if (!string_equals(current_header, entry->header)) {
            if (current_header->length > 0) { string_append(output, "\n"); }
            string_append(output, "[");
            string_append(output, entry->header->c);
            string_append(output, "]\n");
            string_set(current_header, entry->header->c);
        }

        string_append(output, entry->key->c);
        string_append(output, " = ");
        string_append(output, entry->value->c);
        string_append(output, "\n");
    }

    if (!SaveFileText(ini->path->c, output->c)) {
        lle("Failed to save INI file: %s", ini->path->c);
        return;
    }
}
