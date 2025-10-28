#pragma once

#include "common.hpp"
#include "memory.hpp"

struct String {
    union {
        C8 *data;
        C8 *c;
    };
    SZ length;
    SZ capacity;
    MemoryType memory_type;
};

String *string_create_empty(MemoryType memory_type);
String *string_create_with_capacity(MemoryType memory_type, SZ capacity);
String *string_create(MemoryType memory_type, C8 const *format, ...) __attribute__((format(printf, 2, 3)));
String *string_copy(String *s);
String *string_copy_mt(MemoryType memory_type, String *s);
String *string_repeat(MemoryType memory_type, C8 const *cstr, SZ count);
void string_set(String *s, C8 const *cstr);
void string_prepend(String *s, C8 const *cstr);
void string_append(String *s, C8 const *cstr);
void string_appendf(String *s, C8 const *format, ...) __attribute__((format(printf, 2, 3)));
void string_reserve(String *s, SZ new_capacity);
void string_resize(String *s, SZ new_length);
void string_clear(String *s);
void string_insert_char(String *s, C8 c, SZ index);
S32 string_find_char(String *s, C8 c, SZ start_index);
void string_remove_char(String *s, C8 c);
C8 string_pop_back(String *s);
C8 string_pop_front(String *s);
C8 string_peek_back(String *s);
C8 string_peek_front(String *s);
void string_push_back(String *s, C8 c);
void string_push_front(String *s, C8 c);
void string_delete_after_first(String *s, C8 c);
void string_trim_front_space(String *s);
void string_trim_back_space(String *s);
void string_trim_space(String *s);
void string_trim_front(String *s, C8 const *to_trim);
void string_trim_back(String *s, C8 const *to_trim);
void string_trim(String *s, C8 const *to_trim);
BOOL string_empty(String *s);
BOOL string_equals(String *s, String const *other);
BOOL string_equals_cstr(String *s, C8 const *cstr);
BOOL string_starts_with(String *s, String const *prefix);
BOOL string_starts_with_cstr(String *s, C8 const *prefix);
BOOL string_ends_with(String *s, String const *suffix);
BOOL string_ends_with_cstr(String *s, C8 const *suffix);
BOOL string_contains(String *s, String const *sub);
BOOL string_contains_cstr(String *s, C8 const *cstr);
String **string_split(String *s, C8 delimiter, SZ *out_count);
void string_remove_ouc_codes(String *s);
void string_remove_start_to_end(String *s, C8 const *start, C8 const *end);
void string_remove_shell_escape_sequences(String *s);
void string_replace_all(String *s, C8 const *old_cstr, C8 const *new_cstr);
void string_replace_linebreaks(String *s, C8 const *replacement);
void string_to_lower(String *s);
void string_to_upper(String *s);
void string_to_capital(String *s);
void string_truncate(String *s, SZ max_length, C8 const *append);
S32 string_to_s32(String *s);
F32 string_to_f32(String *s);

#define PS(...) string_create(MEMORY_TYPE_PARENA, __VA_ARGS__)
#define TS(...) string_create(MEMORY_TYPE_TARENA, __VA_ARGS__)
