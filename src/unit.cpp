#include "unit.hpp"
#include "std.hpp"

void unit_to_pretty_time_i(S64 value, C8 *buffer, SZ buffer_size, UnitTime max_unit) {
    if (value < 0 || max_unit == UNIT_TIME_NANOSECONDS) {
        ou_snprintf(buffer, buffer_size, "%" PRId64 "ns", value);
        return;
    }

    struct i_helper {
        S64 threshold;
        UnitTime unit;
        C8 const *suffix;
        S64 divisor;
    } static const time_units[] = {
        {1000LL,          UNIT_TIME_MICROSECONDS, "us",   1000LL},
        {1000000LL,       UNIT_TIME_MILLISECONDS, "ms",   1000000LL},
        {1000000000LL,    UNIT_TIME_SECONDS,      "s",    1000000000LL},
        {60000000000LL,   UNIT_TIME_MINUTES,      "min",  60000000000LL},
        {3600000000000LL, UNIT_TIME_HOURS,        "hr",   3600000000000LL},
        {S64_MAX,         UNIT_TIME_DAYS,         "days", 86400000000000LL},
    };

    // Start from largest unit and work down
    for (SZ i = (sizeof(time_units) / sizeof(time_units[0])) - 1; i != 0; i--) {
        if (value >= time_units[i].divisor && max_unit >= time_units[i].unit) {
            ou_snprintf(buffer, buffer_size, "%" PRId64 "%s", value / time_units[i].divisor, time_units[i].suffix);
            return;
        }
    }

    // If no larger unit found, use nanoseconds
    ou_snprintf(buffer, buffer_size, "%" PRId64 "ns", value);
}

void unit_to_pretty_time_u(U64 value, C8 *buffer, SZ buffer_size, UnitTime max_unit) {
    if (value == 0 || max_unit == UNIT_TIME_NANOSECONDS) {
        ou_snprintf(buffer, buffer_size, "%" PRIu64 "%s", value, "ns");
        return;
    }

    struct i_helper {
        U64 threshold;
        UnitTime unit;
        C8 const *suffix;
        U64 divisor;
    } static const time_units[] = {
        {1000ULL,          UNIT_TIME_MICROSECONDS, "us",   1000ULL},
        {1000000ULL,       UNIT_TIME_MILLISECONDS, "ms",   1000000ULL},
        {1000000000ULL,    UNIT_TIME_SECONDS,      "s",    1000000000ULL},
        {60000000000ULL,   UNIT_TIME_MINUTES,      "min",  60000000000ULL},
        {3600000000000ULL, UNIT_TIME_HOURS,        "hr",   3600000000000ULL},
        {U64_MAX,          UNIT_TIME_DAYS,         "days", 86400000000000ULL},
    };

    // Start from largest unit and work down
    for (SZ i = (sizeof(time_units) / sizeof(time_units[0])) - 1; i != 0; i--) {
        if (value >= time_units[i].divisor && max_unit >= time_units[i].unit) {
            ou_snprintf(buffer, buffer_size, "%" PRIu64 "%s", value / time_units[i].divisor, time_units[i].suffix);
            return;
        }
    }

    // If no larger unit found, use nanoseconds
    ou_snprintf(buffer, buffer_size, "%" PRIu64 "ns", value);
}

void unit_to_pretty_time_f(F64 value, C8 *buffer, SZ buffer_size, UnitTime max_unit) {
    if (value < 0.0 || max_unit == UNIT_TIME_NANOSECONDS) {
        ou_snprintf(buffer, buffer_size, "%.2fns", value);
        return;
    }

    // Simple conversion factors from nanoseconds to each unit
    struct i_helper {
        F64 divisor;
        UnitTime unit;
        C8 const *suffix;
    } static const time_units[] = {
        {86400000000000.0, UNIT_TIME_DAYS,         "days"},
        {3600000000000.0,  UNIT_TIME_HOURS,        "hr"},
        {60000000000.0,    UNIT_TIME_MINUTES,      "min"},
        {1000000000.0,     UNIT_TIME_SECONDS,      "s"},
        {1000000.0,        UNIT_TIME_MILLISECONDS, "ms"},
        {1000.0,           UNIT_TIME_MICROSECONDS, "us"},
        {1.0,              UNIT_TIME_NANOSECONDS,  "ns"},
    };

    for (auto const &unit : time_units) {
        if (unit.unit <= max_unit && value >= unit.divisor) {
            ou_snprintf(buffer, buffer_size, "%.2f%s", value / unit.divisor, unit.suffix);
            return;
        }
    }

    // Default to nanoseconds
    ou_snprintf(buffer, buffer_size, "%.2fns", value);
}

void unit_to_pretty_prefix_i(C8 const *unit, S64 value, C8 *buffer, SZ buffer_size, UnitPrefix max_unit) {
    unit_to_pretty_prefix_f(unit, (F64)value, buffer, buffer_size, max_unit);
}

void unit_to_pretty_prefix_u(C8 const *unit, U64 value, C8 *buffer, SZ buffer_size, UnitPrefix max_unit) {
    unit_to_pretty_prefix_f(unit, (F64)value, buffer, buffer_size, max_unit);
}

void unit_to_pretty_prefix_binary_f(C8 const *unit, F64 value, C8 *buffer, SZ buffer_size, UnitPrefixBinary max_unit) {
    if (value < KIBI(1) || max_unit == UNIT_PREFIX_BINARY_NONE) {
        ou_snprintf(buffer, buffer_size, "%.2f%s", value, unit);
        return;
    }

    struct i_helper {
        F64 threshold;
        UnitPrefixBinary prefix;
        C8 const *suffix;
        F64 divisor;
    } static const binary_units[] = {
        {(F64)MEBI(1), UNIT_PREFIX_BINARY_KIBI, "Ki", (F64)KIBI(1)},
        {(F64)GIBI(1), UNIT_PREFIX_BINARY_MEBI, "Mi", (F64)MEBI(1)},
        {(F64)TEBI(1), UNIT_PREFIX_BINARY_GIBI, "Gi", (F64)GIBI(1)},
        {F64_MAX,      UNIT_PREFIX_BINARY_TEBI, "Ti", (F64)TEBI(1)},
    };

    for (auto const &unit_info : binary_units) {
        if (value < unit_info.threshold || max_unit == unit_info.prefix) {
            ou_snprintf(buffer, buffer_size, "%.2f%s%s", value / unit_info.divisor, unit_info.suffix, unit);
            return;
        }
    }
}

void unit_to_pretty_prefix_binary_i(C8 const *unit, S64 value, C8 *buffer, SZ buffer_size, UnitPrefixBinary max_unit) {
    unit_to_pretty_prefix_binary_f(unit, (F64)value, buffer, buffer_size, max_unit);
}

void unit_to_pretty_prefix_binary_u(C8 const *unit, U64 value, C8 *buffer, SZ buffer_size, UnitPrefixBinary max_unit) {
    unit_to_pretty_prefix_binary_f(unit, (F64)value, buffer, buffer_size, max_unit);
}

void unit_to_pretty_prefix_f(C8 const *unit, F64 value, C8 *buffer, SZ buffer_size, UnitPrefix max_unit) {
    if (value < 1000.0 || max_unit == UNIT_PREFIX_NONE) {
        ou_snprintf(buffer, buffer_size, "%.2f%s", value, unit);
        return;
    }

    struct i_helper {
        F64 threshold;
        UnitPrefix prefix;
        C8 const *suffix;
        F64 divisor;
    } static const prefix_units[] = {
        {1e6,     UNIT_PREFIX_KILO, "K", 1e3},
        {1e9,     UNIT_PREFIX_MEGA, "M", 1e6},
        {1e12,    UNIT_PREFIX_GIGA, "G", 1e9},
        {F64_MAX, UNIT_PREFIX_TERA, "T", 1e12},
    };

    for (auto const &unit_info : prefix_units) {
        if (value < unit_info.threshold || max_unit == unit_info.prefix) {
            ou_snprintf(buffer, buffer_size, "%.2f%s%s", value / unit_info.divisor, unit_info.suffix, unit);
            return;
        }
    }
}
