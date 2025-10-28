#include "std.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ou_to_lower(C8 *str) {
    for (SZ i = 0; str[i] != '\0'; ++i) {
        if (str[i] >= 'A' && str[i] <= 'Z') { str[i] += 32; }
    }
}

C8 *ou_strtok(C8 *str, C8 const *delim, C8 **saveptr) {
    if (!str) {
        str = *saveptr;
        if (!str) { return nullptr; }
    }

    str += strspn(str, delim);
    if (!*str) { return (*saveptr = nullptr); }

    *saveptr = str + strcspn(str, delim);
    if (**saveptr) {
        *(*saveptr)++ = '\0';
    } else {
        *saveptr = nullptr;
    }

    return str;
}

S32 ou_sprintf(C8 *str, C8 const *format, ...) {
    va_list args;  // NOLINT
    va_start(args, format);
    S32 const result = vsprintf(str, format, args);
    va_end(args);
    return result;
}

S32 ou_vsprintf(C8 *str, C8 const *format, va_list args) {
    return vsprintf(str, format, args);
}

S32 ou_fprintf(FILE *stream, C8 const *format, ...) {
    va_list args;  // NOLINT
    va_start(args, format);
    S32 const result = vfprintf(stream, format, args);
    va_end(args);
    return result;
}

S32 ou_vfprintf(FILE *stream, C8 const *format, va_list args) {
    return vfprintf(stream, format, args);
}

S32 ou_fflush(FILE *stream) {
    return fflush(stream);
}

S32 ou_snprintf(C8 *str, SZ size, C8 const *format, ...) {
    va_list args;  // NOLINT
    va_start(args, format);
    S32 const result = vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}

S32 ou_vsnprintf(C8 *str, SZ size, C8 const *format, va_list args) {
    return vsnprintf(str, size, format, args);
}

S32 ou_sscanf(C8 const *str, C8 const *format, ...) {
    va_list args;  // NOLINT
    va_start(args, format);
    S32 const result = vsscanf(str, format, args);
    va_end(args);
    return result;
}

C8 const *ou_strchr(C8 const *str, S32 c) {
    return strchr(str, c);
}

C8 const *ou_strrchr(C8 const *str, S32 c) {
    return strrchr(str, c);
}

C8 const *ou_strstr(C8 const *haystack, C8 const *needle) {
    return strstr(haystack, needle);
}

SZ ou_strlen(C8 const *str) {
    return strlen(str);
}

SZ ou_strlen_no_whitespace(C8 const *str) {
    SZ count = 0;
    while (*str) {
        if (*str != ' ' && *str != '\t' && *str != '\n' && *str != '\r') { count++; }
        str++;
    }
    return count;
}

SZ ou_strlen_ouc(C8 const *str) {
    if (str == nullptr) { return 0; }

    SZ length = 0;
    SZ i = 0;

    while (str[i] != '\0') {
        if (str[i] == '\\' && ou_strncmp(&str[i], "\\ouc{", 5) == 0) {
            // Find the end of the escape code
            C8 const *end_escape = ou_strchr(&str[i], '}');
            if (end_escape == nullptr) {
                break;  // No matching '}', exit loop
            }
            // Move past the escape code
            i = (SZ)(end_escape - str + 1);
        } else {
            length++;
            i++;
        }
    }

    return length;
}

S32 ou_strcmp(C8 const *str1, C8 const *str2) {
    return strcmp(str1, str2);
}

S32 ou_strncmp(C8 const *str1, C8 const *str2, SZ num) {
    return strncmp(str1, str2, num);
}

C8 *ou_strncat(C8 *dst, C8 const *src, SZ num) {
    return strncat(dst, src, num);
}

S32 ou_atoi(C8 const *str) {
    return atoi(str);
}

F32 ou_strtof(C8 const *str) {
    return strtof(str, nullptr);
}

F32 ou_strtof_endptr(C8 const *str, C8 **endptr) {
    return strtof(str, endptr);
}

F64 ou_strtod(C8 const *str) {
    return strtod(str, nullptr);
}

F64 ou_strtod_endptr(C8 const *str, C8 **endptr) {
    return strtod(str, endptr);
}

F64 ou_atof(C8 const *str) {
    return atof(str);
}

void *ou_memset(void *ptr, S32 value, SZ num) {
    return memset(ptr, value, num);
}

void *ou_memcpy(void *dst, void const *src, SZ num) {
    return memcpy(dst, src, num);
}

void *ou_memdup(void const *src, SZ num, MemoryType memory_type) {
    void *new_mem = memory_oumalloc(num, memory_type);
    if (new_mem == nullptr) { return nullptr; }
    memcpy(new_mem, src, num);
    return new_mem;
}

void *ou_memmove(void *dst, void const *src, SZ num) {
    return memmove(dst, src, num);
}

S32 ou_memcmp(void const *ptr1, void const *ptr2, SZ num) {
    return memcmp(ptr1, ptr2, num);
}

C8 *ou_strdup(C8 const *str, MemoryType memory_type) {
    if (str == nullptr) { return nullptr; }
    SZ const len = strlen(str) + 1;
    C8 *new_str = (C8 *)memory_oumalloc(len, memory_type);
    if (new_str == nullptr) { return nullptr; }
    memcpy(new_str, str, len);
    return new_str;
}

void ou_strncpy(C8 *dst, C8 const *src, SZ num) {
    strncpy(dst, src, num);
}

C8 *ou_strcpy(C8 *dst, C8 const *src) {
    return strcpy(dst, src);
}

S64 ou_strtol(C8 const *str, C8 **endptr, S32 base) {
    return strtol(str, endptr, base);
}

SZ ou_strspn(C8 const *str, C8 const *accept) {
    return strspn(str, accept);
}

C8 *ou_strn(C8 const *str, SZ num, MemoryType memory_type) {
    C8 *new_str = mm(C8 *, num + 1, memory_type);
    if (new_str == nullptr) { return nullptr; }
    strncpy(new_str, str, num);
    new_str[num] = '\0';
    return new_str;
}

BOOL ou_isspace(C8 c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}
