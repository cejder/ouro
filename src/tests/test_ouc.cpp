#include "asset.hpp"
#include "math.hpp"
#include "test.hpp"

#include <unity.h>

void static test_measure_text_ouc() {
    AFont *font = asset_get_font("spleen-8x16", 16);
    TEST_ASSERT_NOT_NULL(font);
    C8 const *text = "Hello World!";
    C8 const *text_with_ouc = "\\ouc{green}Hello \\ouc{red}World!";
    Vector2 const regular_length = measure_text(font, text);
    Vector2 const ouc_length = measure_text_ouc(font, text_with_ouc);
    TEST_ASSERT_EQUAL_FLOAT(regular_length.x, ouc_length.x);
    TEST_ASSERT_EQUAL_FLOAT(regular_length.y, ouc_length.y);
}

void test_ouc() {
    RUN_TEST(test_measure_text_ouc);
}
