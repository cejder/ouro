#include "test.hpp"
#include "common.hpp"
#include "log.hpp"

#include <unity.h>

void setUp() {}

void tearDown() {}

BOOL test_run() {
    UNITY_BEGIN();

    test_array();
    test_ini();
    test_map();
    test_ouc();
    test_ring();
    test_runtime();
    test_string();
    test_unit();

    S32 const result = UNITY_END();
    if (result == 0) {
        lli("Test result: SUCCESS");
    } else {
        llw("Test result: FAILURE (%d)", result);
    }

    return result == 0;
}
