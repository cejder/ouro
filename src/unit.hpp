#pragma once

#include "common.hpp"

#define PRETTY_BUFFER_SIZE 32
#define NANO_TO_BASE(x)    ((x) * 1e-9)
#define MICRO_TO_BASE(x)   ((x) * 1e-6)
#define MILLI_TO_BASE(x)   ((x) * 1e-3)
#define BASE_TO_NANO(x)    ((x) * 1e9)
#define BASE_TO_MICRO(x)   ((x) * 1e6)
#define BASE_TO_MILLI(x)   ((x) * 1e3)
#define KIBI(x)            ((x) * 1024LL)
#define MEBI(x)            ((x) * 1024LL * 1024LL)
#define GIBI(x)            ((x) * 1024LL * 1024LL * 1024LL)
#define TEBI(x)            ((x) * 1024LL * 1024LL * 1024LL * 1024LL)
#define KILO(x)            ((x) * 1000LL)
#define MEGA(x)            ((x) * 1000000LL)
#define GIGA(x)            ((x) * 1000000000LL)
#define RAD_TO_DEG(x)      ((x) * 57.29577951308232087679815481410517033240547246656432154916)
#define DEG_TO_RAD(x)      ((x) * 0.017453292519943295769236907684886127134428718885417254560)

enum UnitTime : U8 {
    UNIT_TIME_NONE,
    UNIT_TIME_NANOSECONDS,
    UNIT_TIME_MICROSECONDS,
    UNIT_TIME_MILLISECONDS,
    UNIT_TIME_SECONDS,
    UNIT_TIME_MINUTES,
    UNIT_TIME_HOURS,
    UNIT_TIME_DAYS,
};

enum UnitPrefix : U8 {
    UNIT_PREFIX_NONE,
    UNIT_PREFIX_KILO,
    UNIT_PREFIX_MEGA,
    UNIT_PREFIX_GIGA,
    UNIT_PREFIX_TERA,
};

enum UnitPrefixBinary : U8 {
    UNIT_PREFIX_BINARY_NONE,
    UNIT_PREFIX_BINARY_KIBI,
    UNIT_PREFIX_BINARY_MEBI,
    UNIT_PREFIX_BINARY_GIBI,
    UNIT_PREFIX_BINARY_TEBI,
};

void unit_to_pretty_time_i(S64 value, C8 *buffer, SZ buffer_size, UnitTime max_unit);
void unit_to_pretty_time_u(U64 value, C8 *buffer, SZ buffer_size, UnitTime max_unit);
void unit_to_pretty_time_f(F64 value, C8 *buffer, SZ buffer_size, UnitTime max_unit);
void unit_to_pretty_prefix_i(C8 const *unit, S64 value, C8 *buffer, SZ buffer_size, UnitPrefix max_unit);
void unit_to_pretty_prefix_u(C8 const *unit, U64 value, C8 *buffer, SZ buffer_size, UnitPrefix max_unit);
void unit_to_pretty_prefix_f(C8 const *unit, F64 value, C8 *buffer, SZ buffer_size, UnitPrefix max_unit);
void unit_to_pretty_prefix_binary_i(C8 const *unit, S64 value, C8 *buffer, SZ buffer_size, UnitPrefixBinary max_unit);
void unit_to_pretty_prefix_binary_u(C8 const *unit, U64 value, C8 *buffer, SZ buffer_size, UnitPrefixBinary max_unit);
void unit_to_pretty_prefix_binary_f(C8 const *unit, F64 value, C8 *buffer, SZ buffer_size, UnitPrefixBinary max_unit);
