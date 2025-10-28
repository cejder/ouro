#include "ini.hpp"
#include "string.hpp"
#include "test.hpp"

#include <raylib.h>
#include <unity.h>

void static test_ini_file_create() {
    INIFile *ini = ini_file_create(MEMORY_TYPE_TARENA, "test_config.ini", nullptr);
    TEST_ASSERT_NOT_NULL(ini);
    TEST_ASSERT_EQUAL_STRING("test_config.ini", ini->path->c);
    TEST_ASSERT_EQUAL_INT(0, ini->entry_count);
}

void static test_ini_file_set_c8() {
    INIFile *ini = ini_file_create(MEMORY_TYPE_TARENA, "test_config.ini", nullptr);
    ini_file_set_c8(ini, "Settings", "Volume", "75");
    ini_file_set_c8(ini, "Settings", "Brightness", "50.5");
    ini_file_set_c8(ini, "Settings", "Fullscreen", "true");
    C8 const *volume = ini_file_get_c8(ini, "Settings", "Volume");
    TEST_ASSERT_NOT_NULL(volume);
    TEST_ASSERT_EQUAL_STRING("75", volume);
    C8 const *brightness = ini_file_get_c8(ini, "Settings", "Brightness");
    TEST_ASSERT_NOT_NULL(brightness);
    TEST_ASSERT_EQUAL_STRING("50.5", brightness);
    C8 const *fullscreen = ini_file_get_c8(ini, "Settings", "Fullscreen");
    TEST_ASSERT_NOT_NULL(fullscreen);
    TEST_ASSERT_EQUAL_STRING("true", fullscreen);
}

void static test_ini_file_set_s32() {
    INIFile *ini = ini_file_create(MEMORY_TYPE_TARENA, "test_config.ini", nullptr);
    ini_file_set_s32(ini, "Settings", "Volume", 75);
    S32 const volume = ini_file_get_s32(ini, "Settings", "Volume", 0);
    TEST_ASSERT_EQUAL_INT(75, volume);
}

void static test_ini_file_set_f32() {
    INIFile *ini = ini_file_create(MEMORY_TYPE_TARENA, "test_config.ini", nullptr);
    ini_file_set_f32(ini, "Settings", "Brightness", 50.5F);
    F32 const brightness = ini_file_get_f32(ini, "Settings", "Brightness", 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(50.5F, brightness);
}

void static test_ini_file_set_b8() {
    INIFile *ini = ini_file_create(MEMORY_TYPE_TARENA, "test_config.ini", nullptr);
    ini_file_set_b8(ini, "Settings", "Fullscreen", true);
    BOOL fullscreen = ini_file_get_b8(ini, "Settings", "Fullscreen", false);
    TEST_ASSERT_TRUE(fullscreen);
    ini_file_set_b8(ini, "Settings", "Fullscreen", false);
    fullscreen = ini_file_get_b8(ini, "Settings", "Fullscreen", true);
    TEST_ASSERT_FALSE(fullscreen);
}

void static test_ini_file_get_c8() {
    INIFile *ini = ini_file_create(MEMORY_TYPE_TARENA, "test_config.ini", nullptr);
    ini_file_set_c8(ini, "Settings", "foo", "bar");
    C8 const *bar = ini_file_get_c8(ini, "Settings", "foo");
    TEST_ASSERT_EQUAL_STRING("bar", bar);
    C8 const *missing = ini_file_get_c8(ini, "Settings", "Missing");
    TEST_ASSERT_EQUAL_PTR(nullptr, missing);
}

void static test_ini_file_get_s32() {
    INIFile *ini = ini_file_create(MEMORY_TYPE_TARENA, "test_config.ini", nullptr);
    ini_file_set_c8(ini, "Settings", "Volume", "75");
    S32 const volume = ini_file_get_s32(ini, "Settings", "Volume", 100);
    TEST_ASSERT_EQUAL_INT(75, volume);
    S32 const missing = ini_file_get_s32(ini, "Settings", "Missing", 100);
    TEST_ASSERT_EQUAL_INT(100, missing);
}

void static test_ini_file_get_f32() {
    INIFile *ini = ini_file_create(MEMORY_TYPE_TARENA, "test_config.ini", nullptr);
    ini_file_set_c8(ini, "Settings", "Brightness", "50.5");
    F32 const brightness = ini_file_get_f32(ini, "Settings", "Brightness", 1.0F);
    TEST_ASSERT_EQUAL_FLOAT(50.5F, brightness);
    F32 const missing = ini_file_get_f32(ini, "Settings", "Missing", 1.0F);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, missing);
}

void static test_ini_file_get_b8() {
    INIFile *ini = ini_file_create(MEMORY_TYPE_TARENA, "test_config.ini", nullptr);
    ini_file_set_c8(ini, "Settings", "Fullscreen", "true");
    BOOL const fullscreen = ini_file_get_b8(ini, "Settings", "Fullscreen", false);
    TEST_ASSERT_TRUE(fullscreen);
    BOOL const missing = ini_file_get_b8(ini, "Settings", "Missing", false);
    TEST_ASSERT_FALSE(missing);
}

void static test_ini_file_header_exists() {
    INIFile *ini = ini_file_create(MEMORY_TYPE_TARENA, "test_config.ini", nullptr);
    ini_file_set_c8(ini, "Settings", "foo", "bar");
    BOOL const exists = ini_file_header_exists(ini, "Settings");
    TEST_ASSERT_TRUE(exists);
    BOOL const not_exists = ini_file_header_exists(ini, "Missing");
    TEST_ASSERT_FALSE(not_exists);
}

void static test_ini_file_save() {
    INIFile *ini = ini_file_create(MEMORY_TYPE_TARENA, "test_config.ini", nullptr);
    ini_file_set_c8(ini, "Settings", "Volume", "75");
    ini_file_set_c8(ini, "Settings", "Brightness", "50.5");
    ini_file_set_c8(ini, "Settings", "Fullscreen", "true");
    ini_file_save(ini);
    TEST_ASSERT_TRUE(FileExists("test_config.ini"));
    INIFile *loaded_ini = ini_file_create(MEMORY_TYPE_TARENA, "test_config.ini", nullptr);
    TEST_ASSERT_NOT_NULL(loaded_ini);
    TEST_ASSERT_EQUAL_STRING("75", ini_file_get_c8(loaded_ini, "Settings", "Volume"));
    TEST_ASSERT_EQUAL_STRING("50.5", ini_file_get_c8(loaded_ini, "Settings", "Brightness"));
    TEST_ASSERT_EQUAL_STRING("true", ini_file_get_c8(loaded_ini, "Settings", "Fullscreen"));
    TEST_ASSERT_EQUAL_INT(0, remove("test_config.ini"));
}

void test_ini() {
    RUN_TEST(test_ini_file_create);
    RUN_TEST(test_ini_file_set_c8);
    RUN_TEST(test_ini_file_set_s32);
    RUN_TEST(test_ini_file_set_f32);
    RUN_TEST(test_ini_file_set_b8);
    RUN_TEST(test_ini_file_get_c8);
    RUN_TEST(test_ini_file_get_s32);
    RUN_TEST(test_ini_file_get_f32);
    RUN_TEST(test_ini_file_get_b8);
    RUN_TEST(test_ini_file_header_exists);
    RUN_TEST(test_ini_file_save);
}
