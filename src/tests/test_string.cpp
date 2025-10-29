#include "string.hpp"
#include "test.hpp"

#include <unity.h>

void static test_string_create_empty() {
    String *s = string_create_empty(MEMORY_TYPE_ARENA_TRANSIENT);
    TEST_ASSERT_EQUAL_INT(0, s->length);
    TEST_ASSERT_EQUAL_INT(1, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("", s->c);
}

void static test_string_create_with_capacity() {
    String *s = string_create_with_capacity(MEMORY_TYPE_ARENA_TRANSIENT, 100);
    TEST_ASSERT_EQUAL_INT(0, s->length);
    TEST_ASSERT_EQUAL_INT(100, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("", s->c);
}

void static test_string_create() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
}

void static test_string_copy() {
    String *s = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello, world!");
    String *copy_s = string_copy(s);
    TEST_ASSERT_NOT_NULL(copy_s);
    TEST_ASSERT_EQUAL_INT(s->length, copy_s->length);
    TEST_ASSERT_EQUAL_INT(s->capacity, copy_s->capacity);
    TEST_ASSERT_EQUAL_INT(s->memory_type, copy_s->memory_type);
    TEST_ASSERT_EQUAL_STRING(s->c, copy_s->c);
}

void static test_string_copy_mt() {
    String *s = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello, world!");
    String *copy_s = string_copy_mt(MEMORY_TYPE_ARENA_DEBUG, s);
    TEST_ASSERT_NOT_NULL(copy_s);
    TEST_ASSERT_EQUAL_INT(s->length, copy_s->length);
    TEST_ASSERT_EQUAL_INT(s->capacity, copy_s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_DEBUG, copy_s->memory_type);
    TEST_ASSERT_EQUAL_STRING(s->c, copy_s->c);
}

void static test_string_repeat() {
    String *s = string_repeat(MEMORY_TYPE_ARENA_TRANSIENT, "Hello, world!", 3);
    TEST_ASSERT_EQUAL_INT(39, s->length);
    TEST_ASSERT_EQUAL_INT(40, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!Hello, world!Hello, world!", s->c);
}

void static test_string_set() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    string_set(s, "Hello, universe!");
    TEST_ASSERT_EQUAL_INT(16, s->length);
    TEST_ASSERT_EQUAL_INT(17, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, universe!", s->c);
}

void static test_string_prepend() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    string_prepend(s, "Hello, universe!");
    TEST_ASSERT_EQUAL_INT(29, s->length);
    TEST_ASSERT_EQUAL_INT(30, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, universe!Hello, world!", s->c);
}

void static test_string_append() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    string_append(s, "Hello, universe!");
    TEST_ASSERT_EQUAL_INT(29, s->length);
    TEST_ASSERT_EQUAL_INT(30, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!Hello, universe!", s->c);
}

void static test_string_appendf() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    string_appendf(s, "Hello, universe #%d!", 42);
    TEST_ASSERT_EQUAL_INT(33, s->length);
    TEST_ASSERT_EQUAL_INT(34, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!Hello, universe #42!", s->c);
}

void static test_string_reserve() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    string_reserve(s, 100);
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(100, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
}

void static test_string_resize() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    string_resize(s, 100);
    TEST_ASSERT_EQUAL_INT(100, s->length);
    TEST_ASSERT_EQUAL_INT(101, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
}

void static test_string_clear() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    string_clear(s);
    TEST_ASSERT_EQUAL_INT(0, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("", s->c);
}

void static test_string_insert_char() {
    String *s = TS("Hello, world");
    TEST_ASSERT_EQUAL_INT(12, s->length);
    TEST_ASSERT_EQUAL_INT(13, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world", s->c);
    string_insert_char(s, '!', 12);
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
}

void static test_string_find_char() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    S32 const idx = string_find_char(s, 'o', 0);
    TEST_ASSERT_EQUAL_INT(4, idx);
}

void static test_string_remove_char() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    string_remove_char(s, 'o');
    TEST_ASSERT_EQUAL_INT(11, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hell, wrld!", s->c);
}

void static test_string_pop_back() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    C8 const c = string_pop_back(s);
    TEST_ASSERT_EQUAL_INT('!', c);
    TEST_ASSERT_EQUAL_INT(12, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world", s->c);
}

void static test_string_pop_front() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    C8 const c = string_pop_front(s);
    TEST_ASSERT_EQUAL_INT('H', c);
    TEST_ASSERT_EQUAL_INT(12, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("ello, world!", s->c);
}

void static test_string_peek_back() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    C8 const c = string_peek_back(s);
    TEST_ASSERT_EQUAL_INT('!', c);
}

void static test_string_peek_front() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    C8 const c = string_peek_front(s);
    TEST_ASSERT_EQUAL_INT('H', c);
}

void static test_string_push_back() {
    String *s = TS("Hello, world");
    TEST_ASSERT_EQUAL_INT(12, s->length);
    TEST_ASSERT_EQUAL_INT(13, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world", s->c);
    string_push_back(s, '!');
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
}

void static test_string_push_front() {
    String *s = TS("Hello, world");
    TEST_ASSERT_EQUAL_INT(12, s->length);
    TEST_ASSERT_EQUAL_INT(13, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world", s->c);
    string_push_front(s, '!');
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("!Hello, world", s->c);
}

void static test_string_delete_after_first() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    string_delete_after_first(s, 'o');
    TEST_ASSERT_EQUAL_INT(4, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hell", s->c);
}

void static test_string_trim_front_space() {
    String *s = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "   Hello, world!   ");
    string_trim_front_space(s);
    TEST_ASSERT_EQUAL_STRING("Hello, world!   ", s->c);
}

void static test_string_trim_back_space() {
    String *s = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "   Hello, world!   ");
    string_trim_back_space(s);
    TEST_ASSERT_EQUAL_STRING("   Hello, world!", s->c);
}

void static test_string_trim_space() {
    String *s = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "   Hello, world!   ");
    string_trim_space(s);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
}

void static test_string_trim_front() {
    String *s = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "   Hello, world!   ");
    string_trim_front(s, " ");
    TEST_ASSERT_EQUAL_STRING("Hello, world!   ", s->c);
}

void static test_string_trim_back() {
    String *s = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "   Hello, world!   ");
    string_trim_back(s, " ");
    TEST_ASSERT_EQUAL_STRING("   Hello, world!", s->c);
}

void static test_string_trim() {
    String *s = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "   Hello, world!   ");
    string_trim(s, " ");
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
}

void static test_string_empty() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    TEST_ASSERT_EQUAL_INT(0, string_empty(s));
    string_clear(s);
    TEST_ASSERT_EQUAL_INT(1, string_empty(s));
    s = string_create_empty(MEMORY_TYPE_ARENA_TRANSIENT);
    TEST_ASSERT_EQUAL_INT(0, s->length);
    TEST_ASSERT_EQUAL_INT(1, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("", s->c);
    TEST_ASSERT_EQUAL_INT(1, string_empty(s));
}

void static test_string_equals() {
    String *s1 = TS("Hello, world!");
    String *s2 = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s1->length);
    TEST_ASSERT_EQUAL_INT(14, s1->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s1->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s1->c);
    TEST_ASSERT_EQUAL_INT(13, s2->length);
    TEST_ASSERT_EQUAL_INT(14, s2->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s2->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s2->c);
    TEST_ASSERT_EQUAL_INT(1, string_equals(s1, s2));
    string_set(s2, "Hello, universe!");
    TEST_ASSERT_EQUAL_INT(16, s2->length);
    TEST_ASSERT_EQUAL_INT(17, s2->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s2->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, universe!", s2->c);
    TEST_ASSERT_EQUAL_INT(0, string_equals(s1, s2));
}

void static test_string_equals_cstr() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    TEST_ASSERT_EQUAL_INT(1, string_equals_cstr(s, "Hello, world!"));
    TEST_ASSERT_EQUAL_INT(0, string_equals_cstr(s, "Hello, universe!"));
}

void static test_string_starts_with() {
    String *s = TS("Hello, world!");
    String *prefix = TS("Hello");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    TEST_ASSERT_EQUAL_INT(5, prefix->length);
    TEST_ASSERT_EQUAL_INT(6, prefix->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, prefix->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello", prefix->c);
    TEST_ASSERT_EQUAL_INT(1, string_starts_with(s, prefix));
    string_set(prefix, "world");
    TEST_ASSERT_EQUAL_INT(5, prefix->length);
    TEST_ASSERT_EQUAL_INT(6, prefix->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, prefix->memory_type);
    TEST_ASSERT_EQUAL_STRING("world", prefix->c);
    TEST_ASSERT_EQUAL_INT(0, string_starts_with(s, prefix));
}

void static test_string_starts_with_cstr() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    TEST_ASSERT_EQUAL_INT(1, string_starts_with_cstr(s, "Hello"));
    TEST_ASSERT_EQUAL_INT(0, string_starts_with_cstr(s, "world"));
}

void static test_string_ends_with() {
    String *s = TS("Hello, world!");
    String *suffix = TS("world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    TEST_ASSERT_EQUAL_INT(6, suffix->length);
    TEST_ASSERT_EQUAL_INT(7, suffix->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, suffix->memory_type);
    TEST_ASSERT_EQUAL_STRING("world!", suffix->c);
    TEST_ASSERT_EQUAL_INT(1, string_ends_with(s, suffix));
    string_set(suffix, "Hello");
    TEST_ASSERT_EQUAL_INT(5, suffix->length);
    TEST_ASSERT_EQUAL_INT(7, suffix->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, suffix->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello", suffix->c);
    TEST_ASSERT_EQUAL_INT(0, string_ends_with(s, suffix));
}

void static test_string_ends_with_cstr() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    TEST_ASSERT_EQUAL_INT(1, string_ends_with_cstr(s, "world!"));
    TEST_ASSERT_EQUAL_INT(0, string_ends_with_cstr(s, "Hello"));
}

void static test_string_contains() {
    String *s = TS("Hello, world!");
    String *str = TS("world");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    TEST_ASSERT_EQUAL_INT(5, str->length);
    TEST_ASSERT_EQUAL_INT(6, str->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, str->memory_type);
    TEST_ASSERT_EQUAL_STRING("world", str->c);
    TEST_ASSERT_EQUAL_INT(1, string_contains(s, str));
    string_set(str, "Hello");
    TEST_ASSERT_EQUAL_INT(5, str->length);
    TEST_ASSERT_EQUAL_INT(6, str->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, str->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello", str->c);
    TEST_ASSERT_EQUAL_INT(1, string_contains(s, str));
    string_set(str, "universe");
    TEST_ASSERT_EQUAL_INT(8, str->length);
    TEST_ASSERT_EQUAL_INT(9, str->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, str->memory_type);
    TEST_ASSERT_EQUAL_STRING("universe", str->c);
    TEST_ASSERT_EQUAL_INT(0, string_contains(s, str));
}

void static test_string_contains_cstr() {
    String *s = TS("Hello, world!");
    TEST_ASSERT_EQUAL_INT(13, s->length);
    TEST_ASSERT_EQUAL_INT(14, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s->c);
    TEST_ASSERT_EQUAL_INT(1, string_contains_cstr(s, "world"));
    TEST_ASSERT_EQUAL_INT(0, string_contains_cstr(s, "universe"));
}

void static test_string_split() {
    String *s = TS("one,two,three,four");
    TEST_ASSERT_EQUAL_INT(18, s->length);
    TEST_ASSERT_EQUAL_INT(19, s->capacity);
    TEST_ASSERT_EQUAL_INT(MEMORY_TYPE_ARENA_TRANSIENT, s->memory_type);
    TEST_ASSERT_EQUAL_STRING("one,two,three,four", s->c);
    SZ count = 0;
    String **parts = string_split(s, ',', &count);
    TEST_ASSERT_EQUAL_INT(4, count);
    TEST_ASSERT_EQUAL_STRING("one", parts[0]->c);
    TEST_ASSERT_EQUAL_STRING("two", parts[1]->c);
    TEST_ASSERT_EQUAL_STRING("three", parts[2]->c);
    TEST_ASSERT_EQUAL_STRING("four", parts[3]->c);
}

void static test_string_remove_ouc_codes() {
    String *s = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "\\ouc{XXX}Hello, \\ouc{YYY}world! \\ouc{ZZZ}How are you?\\ouc{000} Goodbye!");
    string_remove_ouc_codes(s);
    C8 const *expected = "Hello, world! How are you? Goodbye!";
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING(expected, s->c);
}

void static test_string_remove_start_to_end() {
    String *s = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "ABC({123})DEF({456})GHI");
    string_remove_start_to_end(s, "({", "})");
    C8 const *expected = "ABCDEFGHI";
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING(expected, s->c);
    String *s2 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "ABC({123})DEF({456})GHI({789})");
    string_remove_start_to_end(s2, "({", "})");
    C8 const *expected2 = "ABCDEFGHI";
    TEST_ASSERT_NOT_NULL(s2);
    TEST_ASSERT_EQUAL_STRING(expected2, s2->c);
}

void static test_string_remove_shell_escape_sequences() {
    String *s = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "\033[31mHello, \033[34mworld! \033[32mHow are you?\033[31m Goodbye!");
    string_remove_shell_escape_sequences(s);
    C8 const *expected = "Hello, world! How are you? Goodbye!";
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING(expected, s->c);
}

void static test_string_replace_all() {
    // Test basic replacement
    String *s1 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello, world! Hello, everyone!");
    string_replace_all(s1, "Hello", "Hi");
    TEST_ASSERT_EQUAL_STRING("Hi, world! Hi, everyone!", s1->c);

    // Test replacement with longer string
    String *s2 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "cat cat cat");
    string_replace_all(s2, "cat", "kitten");
    TEST_ASSERT_EQUAL_STRING("kitten kitten kitten", s2->c);

    // Test replacement with shorter string
    String *s3 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "elephant elephant");
    string_replace_all(s3, "elephant", "elk");
    TEST_ASSERT_EQUAL_STRING("elk elk", s3->c);

    // Test replacement with same length string
    String *s4 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "dog dog dog");
    string_replace_all(s4, "dog", "cat");
    TEST_ASSERT_EQUAL_STRING("cat cat cat", s4->c);

    // Test replacement with empty target string
    String *s5 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello, world!");
    string_replace_all(s5, "", "test");
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s5->c);

    // Test replacement with empty replacement string
    String *s6 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello, world!");
    string_replace_all(s6, "o", "");
    TEST_ASSERT_EQUAL_STRING("Hell, wrld!", s6->c);

    // Test replacement with string containing the replacement
    String *s7 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "aaa");
    string_replace_all(s7, "a", "aa");
    TEST_ASSERT_EQUAL_STRING("aaaaaa", s7->c);

    // Test no matches
    String *s8 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello, world!");
    string_replace_all(s8, "xyz", "abc");
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s8->c);

    // Test replacement at start and end
    String *s9 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "###Hello###");
    string_replace_all(s9, "###", "*");
    TEST_ASSERT_EQUAL_STRING("*Hello*", s9->c);

    // Test edge case where old_str is longer than the string
    String *s10 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hi");
    string_replace_all(s10, "Hello", "Bye");
    TEST_ASSERT_EQUAL_STRING("Hi", s10->c);
}

void static test_string_replace_linebreaks() {
    // Test basic \n replacement
    String *s1 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello\nWorld");
    string_replace_linebreaks(s1, " ");
    TEST_ASSERT_EQUAL_STRING("Hello World", s1->c);

    // Test \r\n replacement
    String *s2 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello\r\nWorld\r\nAgain");
    string_replace_linebreaks(s2, " ");
    TEST_ASSERT_EQUAL_STRING("Hello World Again", s2->c);

    // Test mixed \r and \n replacement
    String *s3 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello\rWorld\nAgain\r\nDone");
    string_replace_linebreaks(s3, "_");
    TEST_ASSERT_EQUAL_STRING("Hello_World_Again_Done", s3->c);

    // Test multiple consecutive linebreaks
    String *s4 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello\n\n\nWorld");
    string_replace_linebreaks(s4, ",");
    TEST_ASSERT_EQUAL_STRING("Hello,,,World", s4->c);

    // Test no linebreaks
    String *s5 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello World");
    string_replace_linebreaks(s5, " ");
    TEST_ASSERT_EQUAL_STRING("Hello World", s5->c);

    // Test replacing with longer string
    String *s6 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello\nWorld");
    string_replace_linebreaks(s6, "<br>");
    TEST_ASSERT_EQUAL_STRING("Hello<br>World", s6->c);
}

void static test_string_to_lower() {
    String *s = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello, WORLD! How ARE you 123?");
    string_to_lower(s);
    TEST_ASSERT_EQUAL_STRING("hello, world! how are you 123?", s->c);
}

void static test_string_to_upper() {
    String *s = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello, world! How are you 123?");
    string_to_upper(s);
    TEST_ASSERT_EQUAL_STRING("HELLO, WORLD! HOW ARE YOU 123?", s->c);
}

void static test_string_to_capital() {
    String *s1 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "hello world how are you");
    string_to_capital(s1);
    TEST_ASSERT_EQUAL_STRING("Hello World How Are You", s1->c);

    String *s2 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "HELLO WORLD HOW ARE YOU");
    string_to_capital(s2);
    TEST_ASSERT_EQUAL_STRING("Hello World How Are You", s2->c);

    String *s3 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "hElLo WoRlD hOw ArE yOu");
    string_to_capital(s3);
    TEST_ASSERT_EQUAL_STRING("Hello World How Are You", s3->c);
}

void static test_string_truncate() {
    String *s1 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello, world!");
    string_truncate(s1, 5, "...");
    TEST_ASSERT_EQUAL_STRING("He...", s1->c);
    String *s2 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello, world!");
    string_truncate(s2, 10, "...");
    TEST_ASSERT_EQUAL_STRING("Hello, ...", s2->c);
    String *s3 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "Hello, world!");
    string_truncate(s3, 15, "...");
    TEST_ASSERT_EQUAL_STRING("Hello, world!", s3->c);
}

void static test_string_to_s32() {
    // Test valid positive number
    String *s1 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "123");
    S32 const result1 = string_to_s32(s1);
    TEST_ASSERT_EQUAL_INT32(123, result1);

    // Test valid negative number
    String *s2 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "-456");
    S32 const result2 = string_to_s32(s2);
    TEST_ASSERT_EQUAL_INT32(-456, result2);

    // Test zero
    String *s3 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "0");
    S32 const result3 = string_to_s32(s3);
    TEST_ASSERT_EQUAL_INT32(0, result3);

    // Test invalid string
    String *s4 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "abc");
    S32 const result4 = string_to_s32(s4);
    TEST_ASSERT_EQUAL_INT32(S32_MAX, result4);

    // Test mixed valid/invalid
    String *s5 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "123abc");
    S32 const result5 = string_to_s32(s5);
    TEST_ASSERT_EQUAL_INT32(S32_MAX, result5);

    // Test empty string
    String *s6 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "");
    S32 const result6 = string_to_s32(s6);
    TEST_ASSERT_EQUAL_INT32(S32_MAX, result6);

    // Test null string
    S32 const result7 = string_to_s32(nullptr);
    TEST_ASSERT_EQUAL_INT32(S32_MAX, result7);
}

void static test_string_to_f32() {
    // Test valid positive F32
    String *s1 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "123.45");
    F32 const result1 = string_to_f32(s1);
    TEST_ASSERT_EQUAL_FLOAT(123.45F, result1);

    // Test valid negative F32
    String *s2 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "-67.89");
    F32 const result2 = string_to_f32(s2);
    TEST_ASSERT_EQUAL_FLOAT(-67.89F, result2);

    // Test zero
    String *s3 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "0.0");
    F32 const result3 = string_to_f32(s3);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, result3);

    // Test integer string
    String *s4 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "42");
    F32 const result4 = string_to_f32(s4);
    TEST_ASSERT_EQUAL_FLOAT(42.0F, result4);

    // Test invalid string
    String *s5 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "abc");
    F32 const result5 = string_to_f32(s5);
    TEST_ASSERT_EQUAL_FLOAT(F32_MAX, result5);

    // Test empty string
    String *s6 = string_create(MEMORY_TYPE_ARENA_TRANSIENT, "");
    F32 const result6 = string_to_f32(s6);
    TEST_ASSERT_EQUAL_FLOAT(F32_MAX, result6);

    // Test null string
    F32 const result7 = string_to_f32(nullptr);
    TEST_ASSERT_EQUAL_FLOAT(F32_MAX, result7);
}

void test_string() {
    RUN_TEST(test_string_create_empty);
    RUN_TEST(test_string_create_with_capacity);
    RUN_TEST(test_string_create);
    RUN_TEST(test_string_copy);
    RUN_TEST(test_string_copy_mt);
    RUN_TEST(test_string_repeat);
    RUN_TEST(test_string_set);
    RUN_TEST(test_string_prepend);
    RUN_TEST(test_string_append);
    RUN_TEST(test_string_appendf);
    RUN_TEST(test_string_reserve);
    RUN_TEST(test_string_resize);
    RUN_TEST(test_string_clear);
    RUN_TEST(test_string_insert_char);
    RUN_TEST(test_string_find_char);
    RUN_TEST(test_string_remove_char);
    RUN_TEST(test_string_pop_back);
    RUN_TEST(test_string_pop_front);
    RUN_TEST(test_string_peek_back);
    RUN_TEST(test_string_peek_front);
    RUN_TEST(test_string_push_back);
    RUN_TEST(test_string_push_front);
    RUN_TEST(test_string_delete_after_first);
    RUN_TEST(test_string_trim_front_space);
    RUN_TEST(test_string_trim_back_space);
    RUN_TEST(test_string_trim_space);
    RUN_TEST(test_string_trim_front);
    RUN_TEST(test_string_trim_back);
    RUN_TEST(test_string_trim);
    RUN_TEST(test_string_empty);
    RUN_TEST(test_string_equals);
    RUN_TEST(test_string_equals_cstr);
    RUN_TEST(test_string_starts_with);
    RUN_TEST(test_string_starts_with_cstr);
    RUN_TEST(test_string_ends_with);
    RUN_TEST(test_string_ends_with_cstr);
    RUN_TEST(test_string_contains);
    RUN_TEST(test_string_contains_cstr);
    RUN_TEST(test_string_split);
    RUN_TEST(test_string_remove_ouc_codes);
    RUN_TEST(test_string_remove_start_to_end);
    RUN_TEST(test_string_remove_shell_escape_sequences);
    RUN_TEST(test_string_replace_all);
    RUN_TEST(test_string_replace_linebreaks);
    RUN_TEST(test_string_to_lower);
    RUN_TEST(test_string_to_upper);
    RUN_TEST(test_string_to_capital);
    RUN_TEST(test_string_truncate);
    RUN_TEST(test_string_to_s32);
    RUN_TEST(test_string_to_f32);
}
