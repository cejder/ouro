#include "ease.hpp"
#include "math.hpp"

#include <glm/gtc/type_ptr.hpp>

C8 static const *i_ease_to_cstr_map[EASE_COUNT] = {
    "EASE_LINEAR",
    "EASE_IN_SINE",
    "EASE_IN_QUAD",
    "EASE_IN_CUBIC",
    "EASE_IN_QUART",
    "EASE_IN_QUINT",
    "EASE_IN_EXPO",
    "EASE_IN_CIRC",
    "EASE_IN_BACK",
    "EASE_IN_ELASTIC",
    "EASE_IN_BOUNCE",
    "EASE_OUT_SINE",
    "EASE_OUT_QUAD",
    "EASE_OUT_CUBIC",
    "EASE_OUT_QUART",
    "EASE_OUT_QUINT",
    "EASE_OUT_EXPO",
    "EASE_OUT_CIRC",
    "EASE_OUT_BACK",
    "EASE_OUT_ELASTIC",
    "EASE_OUT_BOUNCE",
    "EASE_IN_OUT_SINE",
    "EASE_IN_OUT_QUAD",
    "EASE_IN_OUT_CUBIC",
    "EASE_IN_OUT_QUART",
    "EASE_IN_OUT_QUINT",
    "EASE_IN_OUT_EXPO",
    "EASE_IN_OUT_CIRC",
    "EASE_IN_OUT_BACK",
    "EASE_IN_OUT_ELASTIC",
    "EASE_IN_OUT_BOUNCE",
};

F32 ease_use(EaseType type, F32 t, F32 b, F32 c, F32 d) {
    switch (type) {
    case EASE_LINEAR:         return ease_linear(t,b,c,d);
    case EASE_IN_SINE:        return ease_in_sine(t,b,c,d);
    case EASE_IN_QUAD:        return ease_in_quad(t,b,c,d);
    case EASE_IN_CUBIC:       return ease_in_cubic(t,b,c,d);
    case EASE_IN_QUART:       return ease_in_quart(t,b,c,d);
    case EASE_IN_QUINT:       return ease_in_quint(t,b,c,d);
    case EASE_IN_EXPO:        return ease_in_expo(t,b,c,d);
    case EASE_IN_CIRC:        return ease_in_circ(t,b,c,d);
    case EASE_IN_BACK:        return ease_in_back(t,b,c,d);
    case EASE_IN_ELASTIC:     return ease_in_elastic(t,b,c,d);
    case EASE_IN_BOUNCE:      return ease_in_bounce(t,b,c,d);
    case EASE_OUT_SINE:       return ease_out_sine(t,b,c,d);
    case EASE_OUT_QUAD:       return ease_out_quad(t,b,c,d);
    case EASE_OUT_CUBIC:      return ease_out_cubic(t,b,c,d);
    case EASE_OUT_QUART:      return ease_out_quart(t,b,c,d);
    case EASE_OUT_QUINT:      return ease_out_quint(t,b,c,d);
    case EASE_OUT_EXPO:       return ease_out_expo(t,b,c,d);
    case EASE_OUT_CIRC:       return ease_out_circ(t,b,c,d);
    case EASE_OUT_BACK:       return ease_out_back(t,b,c,d);
    case EASE_OUT_ELASTIC:    return ease_out_elastic(t,b,c,d);
    case EASE_OUT_BOUNCE:     return ease_out_bounce(t,b,c,d);
    case EASE_IN_OUT_SINE:    return ease_in_out_sine(t,b,c,d);
    case EASE_IN_OUT_QUAD:    return ease_in_out_quad(t,b,c,d);
    case EASE_IN_OUT_CUBIC:   return ease_in_out_cubic(t,b,c,d);
    case EASE_IN_OUT_QUART:   return ease_in_out_quart(t,b,c,d);
    case EASE_IN_OUT_QUINT:   return ease_in_out_quint(t,b,c,d);
    case EASE_IN_OUT_EXPO:    return ease_in_out_expo(t,b,c,d);
    case EASE_IN_OUT_CIRC:    return ease_in_out_circ(t,b,c,d);
    case EASE_IN_OUT_BACK:    return ease_in_out_back(t,b,c,d);
    case EASE_IN_OUT_ELASTIC: return ease_in_out_elastic(t,b,c,d);
    case EASE_IN_OUT_BOUNCE:  return ease_in_out_bounce(t,b,c,d);
    default:                  return ease_linear(t,b,c,d);
    }
}

F32 ease_linear(F32 t, F32 b, F32 c, F32 d) {
    return b + (c * t / d);
}

F32 ease_in_sine(F32 t, F32 b, F32 c, F32 d) {
    return (-c * math_cos_f32(t / d * (glm::pi<F32>() / 2.0F))) + c + b;
}

F32 ease_in_quad(F32 t, F32 b, F32 c, F32 d) {
    t /= d;
    return (c * t * t) + b;
}

F32 ease_in_cubic(F32 t, F32 b, F32 c, F32 d) {
    t /= d;
    return (c * t * t * t) + b;
}

F32 ease_in_quart(F32 t, F32 b, F32 c, F32 d) {
    t /= d;
    return (c * t * t * t * t) + b;
}

F32 ease_in_quint(F32 t, F32 b, F32 c, F32 d) {
    t /= d;
    return (c * t * t * t * t * t) + b;
}

F32 ease_in_expo(F32 t, F32 b, F32 c, F32 d) {
    return (t == 0) ? b : (c * math_pow_f32(2, 10 * (t / d - 1))) + b;
}

F32 ease_in_circ(F32 t, F32 b, F32 c, F32 d) {
    t /= d;
    return (-c * (math_sqrt_f32(1 - (t * t)) - 1)) + b;
}

F32 ease_in_back(F32 t, F32 b, F32 c, F32 d) {
    F32 const s = 1.70158F;
    t /= d;
    return (c * t * t * ((s + 1) * t - s)) + b;
}

F32 ease_in_elastic(F32 t, F32 b, F32 c, F32 d) {
    if (t == 0)        { return b; }
    if ((t /= d) == 1) { return b + c; }
    F32 const p        = d * 0.3F;
    F32 const a        = c;
    F32 const s        = p / 4.0F;
    F32 const post_fix = a * math_pow_f32(2, 10 * (t -= 1));
    return -(post_fix * math_sin_f32((t * d - s) * (2.0F * glm::pi<F32>()) / p)) + b;
}

F32 ease_in_bounce(F32 t, F32 b, F32 c, F32 d) {
    return c - ease_out_bounce(d - t, 0, c, d) + b;
}

F32 ease_out_sine(F32 t, F32 b, F32 c, F32 d) {
    return (c * math_sin_f32(t / d * (glm::pi<F32>() / 2))) + b;
}

F32 ease_out_quad(F32 t, F32 b, F32 c, F32 d) {
    t /= d;
    return (-c * t * (t - 2)) + b;
}

F32 ease_out_cubic(F32 t, F32 b, F32 c, F32 d) {
    t = (t / d) - 1;
    return (c * (t * t * t + 1)) + b;
}

F32 ease_out_quart(F32 t, F32 b, F32 c, F32 d) {
    t = (t / d) - 1;
    return (-c * (t * t * t * t - 1)) + b;
}

F32 ease_out_quint(F32 t, F32 b, F32 c, F32 d) {
    t = (t / d) - 1;
    return (c * (t * t * t * t * t + 1)) + b;
}

F32 ease_out_expo(F32 t, F32 b, F32 c, F32 d) {
    return (t == d) ? b + c : (c * (-math_pow_f32(2, -10 * t / d) + 1)) + b;
}

F32 ease_out_circ(F32 t, F32 b, F32 c, F32 d) {
    t = (t / d) - 1;
    return (c * math_sqrt_f32(1 - (t * t))) + b;
}

F32 ease_out_back(F32 t, F32 b, F32 c, F32 d) {
    F32 const s = 1.70158F;
    t = (t / d) - 1;
    return (c * (t * t * ((s + 1) * t + s) + 1)) + b;
}

F32 ease_out_elastic(F32 t, F32 b, F32 c, F32 d) {
    if (t == 0)        { return b; }
    if ((t /= d) == 1) { return b + c; }
    F32 const p = d * 0.3F;
    F32 const a = c;
    F32 const s = p / 4;
    return ((a * math_pow_f32(2, -10 * t) * math_sin_f32((t * d - s) * (2 * glm::pi<F32>()) / p)) + c + b);
}

F32 ease_out_bounce(F32 t, F32 b, F32 c, F32 d) {
    if ((t /= d) < (1 / 2.75F)) { return (c * (7.5625F * t * t)) + b; }
    if (t < (2 / 2.75F)) {
        F32 const post_fix = t -= (1.5F / 2.75F);
        return (c * (7.5625F * (post_fix)*t + 0.75F)) + b;
    }
    if (t < (2.5F / 2.75F)) {
        F32 const post_fix = t -= (2.25F / 2.75F);
        return (c * (7.5625F * (post_fix)*t + 0.9375F)) + b;
    }
    F32 const post_fix = t -= (2.625F / 2.75F);
    return (c * (7.5625F * (post_fix)*t + 0.984375F)) + b;
}

F32 ease_in_out_sine(F32 t, F32 b, F32 c, F32 d) {
    return (c * math_sin_f32(t / d * (glm::pi<F32>() / 2))) + b;
}

F32 ease_in_out_quad(F32 t, F32 b, F32 c, F32 d) {
    t /= d / 2.0F;
    if (t < 1.0F) { return (c / 2.0F * t * t) + b; }
    t -= 1.0F;
    return (-c / 2.0F * (t * (t - 2.0F) - 1.0F)) + b;
}

F32 ease_in_out_cubic(F32 t, F32 b, F32 c, F32 d) {
    if ((t /= d / 2) < 1) { return (c / 2 * t * t * t) + b; }
    t -= 2;
    return (c / 2 * (t * t * t + 2)) + b;
}

F32 ease_in_out_quart(F32 t, F32 b, F32 c, F32 d) {
    if ((t /= d / 2) < 1) { return (c / 2 * t * t * t * t) + b; }
    t -= 2;
    return (-c / 2 * (t * t * t * t - 2)) + b;
}

F32 ease_in_out_quint(F32 t, F32 b, F32 c, F32 d) {
    if ((t /= d / 2) < 1) { return (c / 2 * t * t * t * t * t) + b; }
    t -= 2;
    return (c / 2 * (t * t * t * t * t + 2)) + b;
}

F32 ease_in_out_expo(F32 t, F32 b, F32 c, F32 d) {
    if (t == 0)           { return b; }
    if (t == d)           { return b + c; }
    if ((t /= d / 2) < 1) { return (c / 2 * math_pow_f32(2, 10 * (t - 1))) + b; }
    return (c / 2 * (-math_pow_f32(2, -10 * --t) + 2)) + b;
}

F32 ease_in_out_circ(F32 t, F32 b, F32 c, F32 d) {
    if ((t /= d / 2) < 1) { return (-c / 2 * (math_sqrt_f32(1 - (t * t)) - 1)) + b; }
    t -= 2;
    return (c / 2 * (math_sqrt_f32(1 - (t * t)) + 1)) + b;
}

F32 ease_in_out_back(F32 t, F32 b, F32 c, F32 d) {
    F32 s = 1.70158F;
    t /= d / 2;
    if (t < 1) {
        s *= 1.525F;
        return (c / 2 * (t * t * ((s + 1) * t - s))) + b;
    }
    F32 const post_fix = t -= 2;
    s *= 1.525F;
    return (c / 2 * ((post_fix)*t * ((s + 1) * t + s) + 2)) + b;
}

F32 ease_in_out_elastic(F32 t, F32 b, F32 c, F32 d) {
    if (t == 0)            { return b; }
    if ((t /= d / 2) == 2) { return b + c; }
    F32 const p = d * (0.3F * 1.5F);
    F32 const a = c;
    F32 const s = p / 4;
    if (t < 1) {
        F32 const post_fix = a * math_pow_f32(2, 10 * (t -= 1));
        return (-0.5F * (post_fix * math_sin_f32((t * d - s) * (2 * glm::pi<F32>()) / p))) + b;
    }
    F32 const post_fix = a * math_pow_f32(2, -10 * (t -= 1));
    return (post_fix * math_sin_f32((t * d - s) * (2 * glm::pi<F32>()) / p) * 0.5F) + c + b;
}

F32 ease_in_out_bounce(F32 t, F32 b, F32 c, F32 d) {
    if (t < d / 2) { return (ease_in_bounce(t * 2, 0, c, d) * 0.5F) + b; }
    return (ease_out_bounce((t * 2) - d, 0, c, d) * 0.5F) + (c * 0.5F) + b;
}

C8 const *ease_to_cstr(EaseType ease_type) {
    return i_ease_to_cstr_map[ease_type];
}
