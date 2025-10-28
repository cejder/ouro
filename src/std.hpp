#pragma once
#include "common.hpp"
#include "memory.hpp"

#ifdef __linux__
#include <bits/types/FILE.h>
#else
#include <stdio.h>
#endif
#include <stdarg.h>

void ou_to_lower(C8 *str);
C8 *ou_strtok(C8 *str, C8 const *delim, C8 **saveptr);
S32 ou_sprintf(C8 *str, C8 const *format, ...) __attribute__((format(printf, 2, 3)));
S32 ou_vsprintf(C8 *str, C8 const *format, va_list args) __attribute__((format(printf, 2, 0)));
S32 ou_fprintf(FILE *stream, C8 const *format, ...) __attribute__((format(printf, 2, 3)));
S32 ou_vfprintf(FILE *stream, C8 const *format, va_list args) __attribute__((format(printf, 2, 0)));
S32 ou_fflush(FILE *stream);
S32 ou_snprintf(C8 *str, SZ size, C8 const *format, ...) __attribute__((format(printf, 3, 4)));
S32 ou_vsnprintf(C8 *str, SZ size, C8 const *format, va_list args) __attribute__((format(printf, 3, 0)));
S32 ou_sscanf(C8 const *str, C8 const *format, ...) __attribute__((format(scanf, 2, 3)));
C8 const *ou_strchr(C8 const *str, S32 c);
C8 const *ou_strrchr(C8 const *str, S32 c);
C8 const *ou_strstr(C8 const *haystack, C8 const *needle);
SZ ou_strlen(C8 const *str);
SZ ou_strlen_no_whitespace(C8 const *str);
SZ ou_strlen_ouc(C8 const *str);
S32 ou_strcmp(C8 const *str1, C8 const *str2);
S32 ou_strncmp(C8 const *str1, C8 const *str2, SZ num);
C8 *ou_strncat(C8 *dst, C8 const *src, SZ num);
S32 ou_atoi(C8 const *str);
F32 ou_strtof(C8 const *str);
F32 ou_strtof_endptr(C8 const *str, C8 **endptr);
F64 ou_strtod(C8 const *str);
F64 ou_strtod_endptr(C8 const *str, C8 **endptr);
F64 ou_atof(C8 const *str);
void *ou_memset(void *ptr, S32 value, SZ num);
void *ou_memcpy(void *dst, const void *src, SZ num);
void *ou_memdup(const void *src, SZ num, MemoryType memory_type);
void *ou_memmove(void *dst, const void *src, SZ num);
S32 ou_memcmp(const void *ptr1, const void *ptr2, SZ num);
C8 *ou_strdup(C8 const *str, MemoryType memory_type);
void ou_strncpy(C8 *dst, C8 const *src, SZ num);
C8 *ou_strcpy(C8 *dst, C8 const *src);
S64 ou_strtol(C8 const *str, C8 **endptr, S32 base);
SZ ou_strspn(C8 const *str, C8 const *accept);
C8 *ou_strn(C8 const *str, SZ num, MemoryType memory_type);
BOOL ou_isspace(C8 c);
