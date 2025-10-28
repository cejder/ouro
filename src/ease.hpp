#pragma once

#include "common.hpp"

enum EaseType : U8 {
    EASE_LINEAR,
    EASE_IN_SINE,
    EASE_IN_QUAD,
    EASE_IN_CUBIC,
    EASE_IN_QUART,
    EASE_IN_QUINT,
    EASE_IN_EXPO,
    EASE_IN_CIRC,
    EASE_IN_BACK,
    EASE_IN_ELASTIC,
    EASE_IN_BOUNCE,
    EASE_OUT_SINE,
    EASE_OUT_QUAD,
    EASE_OUT_CUBIC,
    EASE_OUT_QUART,
    EASE_OUT_QUINT,
    EASE_OUT_EXPO,
    EASE_OUT_CIRC,
    EASE_OUT_BACK,
    EASE_OUT_ELASTIC,
    EASE_OUT_BOUNCE,
    EASE_IN_OUT_SINE,
    EASE_IN_OUT_QUAD,
    EASE_IN_OUT_CUBIC,
    EASE_IN_OUT_QUART,
    EASE_IN_OUT_QUINT,
    EASE_IN_OUT_EXPO,
    EASE_IN_OUT_CIRC,
    EASE_IN_OUT_BACK,
    EASE_IN_OUT_ELASTIC,
    EASE_IN_OUT_BOUNCE,
    EASE_COUNT,
};

// INFO:
// type  The type of easing curve to apply (e.g., Linear, EaseIn, EaseOut, etc.).
// t     The current time or progress of the animation, ranging from 0 to d.
// b     The starting value of the property being animated.
// c     The total change in the value by the end of the animation (final value - start value).
// d     The total duration of the animation.

F32 ease_use(EaseType type, F32 t, F32 b, F32 c, F32 d);
F32 ease_linear(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_sine(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_quad(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_cubic(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_quart(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_quint(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_expo(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_circ(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_back(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_elastic(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_bounce(F32 t, F32 b, F32 c, F32 d);
F32 ease_out_sine(F32 t, F32 b, F32 c, F32 d);
F32 ease_out_quad(F32 t, F32 b, F32 c, F32 d);
F32 ease_out_cubic(F32 t, F32 b, F32 c, F32 d);
F32 ease_out_quart(F32 t, F32 b, F32 c, F32 d);
F32 ease_out_quint(F32 t, F32 b, F32 c, F32 d);
F32 ease_out_expo(F32 t, F32 b, F32 c, F32 d);
F32 ease_out_circ(F32 t, F32 b, F32 c, F32 d);
F32 ease_out_back(F32 t, F32 b, F32 c, F32 d);
F32 ease_out_elastic(F32 t, F32 b, F32 c, F32 d);
F32 ease_out_bounce(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_out_sine(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_out_quad(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_out_cubic(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_out_quart(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_out_quint(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_out_expo(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_out_circ(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_out_back(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_out_elastic(F32 t, F32 b, F32 c, F32 d);
F32 ease_in_out_bounce(F32 t, F32 b, F32 c, F32 d);
C8 const *ease_to_cstr(EaseType ease_type);
