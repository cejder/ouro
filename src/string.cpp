#include "string.hpp"

#include "log.hpp"
#include "std.hpp"

String *string_create_empty(MemoryType memory_type) {
    auto *s = mm(String *, sizeof(String), memory_type);
    if (!s) {
        lle("Could not allocate memory for string");
        return nullptr;
    }

    s->length      = 0;
    s->capacity    = 1;  // Include space for null terminator.
    s->memory_type = memory_type;
    s->data        = mm(C8 *, s->capacity, s->memory_type);
    if (!s->data) {
        lle("Could not allocate memory for string data");
        return nullptr;
    }

    s->data[0] = '\0';

    return s;
}

String *string_create_with_capacity(MemoryType memory_type, SZ capacity) {
    auto *s = mm(String *, sizeof(String), memory_type);
    if (!s) {
        lle("Could not allocate memory for string");
        return nullptr;
    }

    s->length      = 0;
    s->capacity    = capacity;
    s->memory_type = memory_type;
    s->data        = mm(C8 *, s->capacity, s->memory_type);
    if (!s->data) {
        lle("Could not allocate memory for string data");
        return nullptr;
    }

    s->data[0] = '\0';

    return s;
}

String *string_create(MemoryType memory_type, C8 const *format, ...) {
    auto *s = mm(String *, sizeof(String), memory_type);
    if (!s) {
        lle("Could not allocate memory for string: %s", format);
        return nullptr;
    }

    va_list args;  // NOLINT
    va_start(args, format);
    s->length      = (SZ)ou_vsnprintf(nullptr, 0, format, args);
    s->capacity    = s->length + 1;  // Include space for null terminator.
    s->memory_type = memory_type;
    s->data        = mm(C8 *, s->capacity, s->memory_type);
    va_end(args);

    if (!s->data) {
        lle("Could not allocate memory for string: %s", format);
        return nullptr;
    }

    va_start(args, format);
    ou_vsnprintf(s->data, s->capacity, format, args);
    va_end(args);

    return s;
}

String *string_copy(String *s) {
    auto *new_s = mm(String *, sizeof(String), s->memory_type);
    if (!new_s) {
        lle("Could not allocate memory for string copy: %s", s->data);
        return nullptr;
    }

    new_s->data = mm(C8 *, s->capacity, s->memory_type);
    if (!new_s->data) {
        lle("Could not allocate memory for string copy data: %s", s->data);
        return nullptr;
    }

    ou_memcpy(new_s->data, s->data, s->capacity);
    new_s->length      = s->length;
    new_s->capacity    = s->capacity;
    new_s->memory_type = s->memory_type;

    return new_s;
}

String *string_copy_mt(MemoryType memory_type, String *s) {
    auto *new_s = mm(String *, sizeof(String), memory_type);
    if (!new_s) {
        lle("Could not allocate memory for string copy: %s", s->data);
        return nullptr;
    }

    new_s->data = mm(C8 *, s->capacity, memory_type);
    if (!new_s->data) {
        lle("Could not allocate memory for string copy data: %s", s->data);
        return nullptr;
    }

    ou_memcpy(new_s->data, s->data, s->capacity);
    new_s->length      = s->length;
    new_s->capacity    = s->capacity;
    new_s->memory_type = memory_type;

    return new_s;
}

String *string_repeat(MemoryType memory_type, C8 const *cstr, SZ count) {
    auto *s = mm(String *, sizeof(String), memory_type);
    if (!s) {
        lle("Could not allocate memory for string: %s", cstr);
        return nullptr;
    }

    SZ const length = ou_strlen(cstr);
    s->capacity     = length * count + 1;  // Include space for null terminator.
    s->memory_type  = memory_type;
    s->data         = mm(C8 *, s->capacity, s->memory_type);

    if (!s->data) {
        lle("Could not allocate memory for string: %s", cstr);
        return nullptr;
    }

    for (SZ i = 0; i < count; ++i) { ou_memcpy(s->data + (i * length), (void *)cstr, length); }

    s->length          = length * count;
    s->data[s->length] = '\0';

    return s;
}

void string_set(String *s, C8 const *cstr) {
    SZ const length = ou_strlen(cstr);
    if (length + 1 > s->capacity) { string_reserve(s, length + 1); }
    ou_memcpy(s->data, cstr, length + 1);
    s->length = length;
}

void string_prepend(String *s, C8 const *cstr) {
    SZ const length = ou_strlen(cstr);
    if (s->length + length + 1 > s->capacity) { string_reserve(s, s->length + length + 1); }
    ou_memmove(s->data + length, s->data, s->length + 1);
    ou_memcpy(s->data, (void *)cstr, length);
    s->length += length;
}

void string_append(String *s, C8 const *cstr) {
    SZ const length = ou_strlen(cstr);
    if (s->length + length + 1 > s->capacity) { string_reserve(s, s->length + length + 1); }
    ou_memcpy(s->data + s->length, (void *)cstr, length + 1);
    s->length += length;
}

void string_appendf(String *s, C8 const *format, ...) {
    va_list args;  // NOLINT

    // First, calculate the required length
    va_start(args, format);
    SZ const append_length = (SZ)ou_vsnprintf(nullptr, 0, format, args);
    va_end(args);

    // Check if we need to resize
    if (s->length + append_length + 1 > s->capacity) { string_reserve(s, s->length + append_length + 1); }

    // Now do the actual formatting directly into our buffer at the current position
    va_start(args, format);
    ou_vsnprintf(s->data + s->length, append_length + 1, format, args);
    va_end(args);

    s->length += append_length;
}

void string_reserve(String *s, SZ new_capacity) {
    if (new_capacity <= s->capacity) { return; }
    C8 *new_data = nullptr;
    new_data     = mm(C8 *, new_capacity, s->memory_type);
    if (!new_data) {
        lle("Could not allocate memory for string: %s", s->data);
        return;
    }
    ou_memcpy(new_data, s->data, s->length + 1);
    s->data     = new_data;
    s->capacity = new_capacity;
}

void string_resize(String *s, SZ new_length) {
    if (new_length <= s->length) {
        s->data[new_length] = '\0';
        s->length = new_length;
        return;
    }
    string_reserve(s, new_length + 1);
    ou_memset(s->data + s->length, 0, new_length - s->length);
    s->length = new_length;
}

void string_clear(String *s) {
    s->data[0] = '\0';
    s->length  = 0;
}

void string_insert_char(String *s, C8 c, SZ index) {
    if (index >= s->length) {
        string_push_back(s, c);
        return;
    }

    if (s->length + 1 >= s->capacity) { string_reserve(s, s->length + 2); }
    ou_memmove(s->data + index + 1, s->data + index, s->length - index + 1);
    s->data[index] = c;
    ++s->length;
}

S32 string_find_char(String *s, C8 c, SZ start_index) {
    for (SZ i = start_index; i < s->length; ++i) {
        if (s->data[i] == c) { return (S32)i; }
    }
    return -1;
}

void string_remove_char(String *s, C8 c) {
    SZ j = 0;
    for (SZ i = 0; i < s->length; ++i) {
        if (s->data[i] != c) { s->data[j++] = s->data[i]; }
    }
    s->data[j] = '\0';
    s->length  = j;
}

C8 string_pop_back(String *s) {
    if (s->length == 0) { return '\0'; }
    C8 const c = s->data[s->length - 1];
    s->data[s->length - 1] = '\0';
    --s->length;
    return c;
}

C8 string_pop_front(String *s) {
    if (s->length == 0) { return '\0'; }
    C8 const c = s->data[0];
    ou_memmove(s->data, s->data + 1, s->length);
    s->data[s->length - 1] = '\0';
    --s->length;
    return c;
}

C8 string_peek_back(String *s) {
    if (s->length == 0) { return '\0'; }
    return s->data[s->length - 1];
}

C8 string_peek_front(String *s) {
    if (s->length == 0) { return '\0'; }
    return s->data[0];
}

void string_push_back(String *s, C8 c) {
    if (s->length + 1 >= s->capacity) { string_reserve(s, s->length + 2); }
    s->data[s->length]     = c;
    s->data[s->length + 1] = '\0';
    ++s->length;
}

void string_push_front(String *s, C8 c) {
    if (s->length + 1 >= s->capacity) { string_reserve(s, s->length + 2); }
    ou_memmove(s->data + 1, s->data, s->length + 1);
    s->data[0] = c;
    ++s->length;
}

void string_delete_after_first(String *s, C8 c) {
    SZ i = 0;
    for (; i < s->length; ++i) {
        if (s->data[i] == c) { break; }
    }
    s->data[i] = '\0';
    s->length  = i;
}

void string_trim_front_space(String *s) {
    SZ start = 0;
    while (start < s->length && (s->data[start] == ' ' || s->data[start] == '\t' || s->data[start] == '\n' || s->data[start] == '\r')) { start++; }

    if (start > 0) {
        SZ const new_length = s->length - start;
        ou_memmove(s->data, s->data + start, new_length);
        s->data[new_length] = '\0';
        s->length           = new_length;
    }
}

void string_trim_back_space(String *s) {
    SZ end = s->length;
    while (end > 0 && (s->data[end - 1] == ' ' || s->data[end - 1] == '\t' || s->data[end - 1] == '\n' || s->data[end - 1] == '\r')) { end--; }

    if (end < s->length) {
        s->data[end] = '\0';
        s->length    = end;
    }
}

void string_trim_space(String *s) {
    string_trim_front_space(s);
    string_trim_back_space(s);
}

void string_trim_front(String *s, C8 const *to_trim) {
    SZ start = 0;
    while (start < s->length && ou_strchr(to_trim, s->data[start])) { start++; }

    if (start > 0) {
        SZ const new_length = s->length - start;
        ou_memmove(s->data, s->data + start, new_length);
        s->data[new_length] = '\0';
        s->length           = new_length;
    }
}

void string_trim_back(String *s, C8 const *to_trim) {
    SZ end = s->length;
    while (end > 0 && ou_strchr(to_trim, s->data[end - 1])) { end--; }

    if (end < s->length) {
        s->data[end] = '\0';
        s->length    = end;
    }
}

void string_trim(String *s, C8 const *to_trim) {
    string_trim_front(s, to_trim);
    string_trim_back(s, to_trim);
}

BOOL string_empty(String *s) {
    return s->length == 0;
}

BOOL string_equals(String *s, String const *other) {
    if (s->length != other->length) { return false; }

    for (SZ i = 0; i < s->length; ++i) {
        if (s->data[i] != other->data[i]) { return false; }
    }

    return true;
}

BOOL string_equals_cstr(String *s, C8 const *cstr) {
    SZ const length = ou_strlen(cstr);
    if (s->length != length) { return false; }

    for (SZ i = 0; i < s->length; ++i) {
        if (s->data[i] != cstr[i]) { return false; }
    }

    return true;
}

BOOL string_starts_with(String *s, String const *prefix) {
    if (s->length < prefix->length) { return false; }

    for (SZ i = 0; i < prefix->length; ++i) {
        if (s->data[i] != prefix->data[i]) { return false; }
    }

    return true;
}

BOOL string_starts_with_cstr(String *s, C8 const *prefix) {
    SZ const length = ou_strlen(prefix);
    if (s->length < length) { return false; }

    for (SZ i = 0; i < length; ++i) {
        if (s->data[i] != prefix[i]) { return false; }
    }

    return true;
}

BOOL string_ends_with(String *s, String const *suffix) {
    if (s->length < suffix->length) { return false; }

    for (SZ i = 0; i < suffix->length; ++i) {
        if (s->data[s->length - suffix->length + i] != suffix->data[i]) { return false; }
    }

    return true;
}

BOOL string_ends_with_cstr(String *s, C8 const *suffix) {
    SZ const length = ou_strlen(suffix);
    if (s->length < length) { return false; }

    for (SZ i = 0; i < length; ++i) {
        if (s->data[s->length - length + i] != suffix[i]) { return false; }
    }

    return true;
}

BOOL string_contains(String *s, String const *sub) {
    return ou_strstr(s->data, sub->data) != nullptr;
}

BOOL string_contains_cstr(String *s, C8 const *cstr) {
    return ou_strstr(s->data, cstr) != nullptr;
}

String **string_split(String *s, C8 delimiter, SZ *out_count) {
    SZ count = 1;  // At least one part even if no delimiter is found
    for (SZ i = 0; i < s->length; ++i) {
        if (s->data[i] == delimiter) { count++; }
    }

    auto **parts = mm(String **, count * sizeof(String *), s->memory_type);
    if (!parts) {
        lle("Could not allocate memory for string parts: %s", s->data);
        return nullptr;
    }

    SZ part_start = 0;
    SZ part_idx   = 0;
    for (SZ i = 0; i <= s->length; ++i) {
        if (s->data[i] == delimiter || i == s->length) {
            SZ const part_length = i - part_start;
            parts[part_idx] = mm(String *, sizeof(String), s->memory_type);
            if (!parts[part_idx]) {
                lle("Could not allocate memory for string part: %s", s->data);
                return nullptr;
            }
            parts[part_idx]->data = mm(C8 *, part_length + 1, s->memory_type);
            if (!parts[part_idx]->data) {
                lle("Could not allocate memory for string part data: %s", s->data);
                return nullptr;
            }
            ou_memcpy(parts[part_idx]->data, s->data + part_start, part_length);
            parts[part_idx]->data[part_length] = '\0';
            parts[part_idx]->length = part_length;
            parts[part_idx]->capacity = part_length + 1;
            parts[part_idx]->memory_type = s->memory_type;

            part_start = i + 1;
            part_idx++;
        }
    }

    *out_count = count;
    return parts;
}

void string_remove_ouc_codes(String *s) {
    C8 const start_seq[] = "\\ouc{";
    C8 const end_seq[]   = "}";
    string_remove_start_to_end(s, start_seq, end_seq);
}

void string_remove_start_to_end(String *s, C8 const *start, C8 const *end) {
    SZ const start_len = ou_strlen(start);
    SZ const end_len   = ou_strlen(end);

    SZ i = 0;  // Current read idx
    SZ j = 0;  // Current write idx

    while (i < s->length) {
        // Check if the start sequence matches
        if (i + start_len <= s->length && ou_memcmp(s->data + i, start, start_len) == 0) {
            // Move idx past the start sequence
            i += start_len;

            // Find the end sequence
            SZ end_idx = i;
            while (end_idx + end_len <= s->length && ou_memcmp(s->data + end_idx, end, end_len) != 0) { end_idx++; }

            // If end sequence is found, move idx past the end sequence
            if (end_idx + end_len <= s->length) {
                end_idx += end_len;  // Move idx past end sequence
            } else {
                end_idx = s->length;  // If no end sequence is found, set end_idx to length of the string
            }

            // Skip the OUC code
            i = end_idx;
        } else {
            // Copy characters that are not part of an OUC code
            s->data[j++] = s->data[i++];
        }
    }

    // Null terminate the string and update length
    s->data[j] = '\0';
    s->length  = j;
}

void string_remove_shell_escape_sequences(String *s) {
    SZ i = 0;  // Current read idx
    SZ j = 0;  // Current write idx

    while (i < s->length) {
        // Check if the start sequence matches (i.e., '\033[' or '\e[')
        if (s->data[i] == '\e' && i + 1 < s->length && s->data[i + 1] == '[') {
            // Move idx past the escape sequence
            i += 2;  // Skip the '\e' and '[' characters

            // Skip until we find 'm' or reach the end of the string
            while (i < s->length && s->data[i] != 'm') { i++; }

            // Skip the 'm' character if we found it
            if (i < s->length) { i++; }
        } else {
            // Copy characters that are not part of an escape sequence
            s->data[j++] = s->data[i++];
        }
    }

    // Null-terminate the modified string and update its length
    s->data[j] = '\0';
    s->length  = j;
}

void string_replace_all(String *s, C8 const *old_cstr, C8 const *new_cstr) {
    if (!old_cstr || !new_cstr || old_cstr[0] == '\0') { return; }

    SZ const old_len = ou_strlen(old_cstr);
    SZ const new_len = ou_strlen(new_cstr);

    // If old string is longer than our string, no matches possible
    if (old_len > s->length) { return; }

    // First pass: count occurrences
    SZ count = 0;
    for (SZ i = 0; i <= s->length - old_len; i++) {
        if (ou_memcmp(s->data + i, old_cstr, old_len) == 0) { count++; }
    }

    if (count == 0) {
        return;  // Nothing to replace
    }

    // Calculate new size and reallocate if needed
    SZ const new_total_len = s->length + (count * (new_len - old_len));
    C8 *new_data           = mm(C8 *, new_total_len + 1, s->memory_type);
    if (!new_data) {
        lle("Could not allocate memory for string replacement");
        return;
    }

    // Second pass: perform replacements
    SZ read  = 0;
    SZ write = 0;

    while (read < s->length) {
        if (read <= s->length - old_len && ou_memcmp(s->data + read, old_cstr, old_len) == 0) {
            // Copy replacement string
            ou_memcpy(new_data + write, new_cstr, new_len);
            write += new_len;
            read  += old_len;
        } else {
            // Copy original character
            new_data[write++] = s->data[read++];
        }
    }

    // Update string with new data
    s->data            = new_data;
    s->length          = write;
    s->capacity        = new_total_len + 1;
    s->data[s->length] = '\0';
}

void string_replace_linebreaks(String *s, C8 const *replacement) {
    if (!replacement) { return; }

    // First pass: count line breaks
    SZ count = 0;
    for (SZ i = 0; i < s->length; i++) {
        if (s->data[i] == '\n') {
            count++;
        } else if (s->data[i] == '\r') {
            // Handle \r\n as a single line break
            if (i + 1 < s->length && s->data[i + 1] == '\n') {
                count++;
                i++;
            } else {
                count++;
            }
        }
    }

    if (count == 0) {
        return;  // Nothing to replace
    }

    SZ const replacement_len = ou_strlen(replacement);
    SZ const new_total_len   = s->length + (count * (replacement_len - 1));  // -1 because each linebreak is 1 character
    C8 *new_data             = mm(C8 *, new_total_len + 1, s->memory_type);
    if (!new_data) {
        lle("Could not allocate memory for linebreak replacement");
        return;
    }

    // Second pass: perform replacements
    SZ read  = 0;
    SZ write = 0;

    while (read < s->length) {
        if (s->data[read] == '\n') {
            ou_memcpy(new_data + write, replacement, replacement_len);
            write += replacement_len;
            read++;
        } else if (s->data[read] == '\r') {
            if (read + 1 < s->length && s->data[read + 1] == '\n') {
                ou_memcpy(new_data + write, replacement, replacement_len);
                write += replacement_len;
                read  += 2;  // Skip both \r and \n
            } else {
                ou_memcpy(new_data + write, replacement, replacement_len);
                write += replacement_len;
                read  += 1;
            }
        } else {
            new_data[write++] = s->data[read++];
        }
    }

    s->data            = new_data;
    s->length          = write;
    s->capacity        = new_total_len + 1;
    s->data[s->length] = '\0';
}

void string_to_lower(String *s) {
    for (SZ i = 0; i < s->length; ++i) {
        if (s->data[i] >= 'A' && s->data[i] <= 'Z') { s->data[i] = (C8)(s->data[i] + ('a' - 'A')); }
    }
}

void string_to_upper(String *s) {
    for (SZ i = 0; i < s->length; ++i) {
        if (s->data[i] >= 'a' && s->data[i] <= 'z') { s->data[i] = (C8)(s->data[i] + ('A' - 'a')); }
    }
}

void string_to_capital(String *s) {
    // Handle first character if string is not empty
    if (s->length > 0 && s->data[0] >= 'a' && s->data[0] <= 'z') { s->data[0] = (C8)(s->data[0] + ('A' - 'a')); }

    // Process rest of string - capitalize first letter after spaces
    for (SZ i = 1; i < s->length; ++i) {
        if (s->data[i - 1] == ' ' && s->data[i] >= 'a' && s->data[i] <= 'z') {
            s->data[i] = (C8)(s->data[i] + ('A' - 'a'));
        }
        // Convert any other uppercase letters to lowercase
        else if (s->data[i] >= 'A' && s->data[i] <= 'Z' && s->data[i - 1] != ' ') {
            s->data[i] = (C8)(s->data[i] + ('a' - 'A'));
        }
    }
}

void string_truncate(String *s, SZ max_length, C8 const *append) {
    if (s->length <= max_length) { return; }

    SZ const append_length = ou_strlen(append);
    if (max_length < append_length) {
        string_resize(s, max_length);
        return;
    }

    SZ const new_length = max_length - append_length;
    string_resize(s, new_length);
    string_append(s, append);
}

S32 string_to_s32(String *s) {
    if (!s || !s->data || s->length == 0) { return S32_MAX; }

    C8 *endptr       = nullptr;
    S32 const result = (S32)ou_strtol(s->data, &endptr, 10);

    // Check if conversion was successful
    if (endptr == s->data || *endptr != '\0') {
        // Conversion failed or string contains non-numeric characters
        return S32_MAX;
    }

    return result;
}

F32 string_to_f32(String *s) {
    if (!s || !s->data || s->length == 0) { return F32_MAX; }

    C8 *endptr       = nullptr;
    F32 const result = ou_strtof_endptr(s->data, &endptr);

    // Check if conversion was successful
    if (endptr == s->data || *endptr != '\0') {
        // Conversion failed or string contains non-numeric characters
        return F32_MAX;
    }

    return result;
}
