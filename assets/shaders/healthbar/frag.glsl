#version 460

in vec2 frag_uv;
flat in float frag_health_perc;
flat in vec2 frag_size;

out vec4 color;

uniform vec4 u_bg_color;
uniform vec4 u_border_color;
uniform float u_border_thickness;
uniform float u_time;

// Color constants (matching C++ side)
const vec3 PERSIMMON = vec3(255.0, 94.0, 72.0) / 255.0;
const vec3 TANGERINE = vec3(255.0, 140.0, 80.0) / 255.0;
const vec3 SUNSETAMBER = vec3(255.0, 183.0, 77.0) / 255.0;
const vec3 SPRINGLEAF = vec3(120.0, 220.0, 150.0) / 255.0;

// Easing functions
float ease_in_out_cubic(float t) {
    return t < 0.5 ? 4.0 * t * t * t : 1.0 - pow(-2.0 * t + 2.0, 3.0) / 2.0;
}

float ease_in_out_sine(float t) {
    return -(cos(3.14159265 * t) - 1.0) / 2.0;
}

// Color helper functions
vec3 color_lerp(vec3 a, vec3 b, float t) {
    return mix(a, b, t);
}

vec3 color_saturated(vec3 c) {
    // Increase saturation
    float gray = dot(c, vec3(0.299, 0.587, 0.114));
    return mix(vec3(gray), c, 1.5);
}

vec3 color_blend(vec3 a, vec3 b) {
    return (a + b) * 0.5;
}

// Calculate health bar color based on health percentage
vec3 calculate_health_color(float health_perc) {
    if (health_perc > 0.7) {
        return SPRINGLEAF;
    } else if (health_perc > 0.4) {
        float t = (health_perc - 0.4) / 0.3;
        return color_lerp(SUNSETAMBER, SPRINGLEAF, ease_in_out_cubic(t));
    } else if (health_perc > 0.15) {
        float t = (health_perc - 0.15) / 0.25;
        return color_lerp(PERSIMMON, SUNSETAMBER, ease_in_out_cubic(t));
    } else {
        // Critical health - pulsing animation
        float pulse = ease_in_out_sine((sin(u_time * 8.0) + 1.0) * 0.5);
        vec3 critical_base = color_saturated(PERSIMMON);
        vec3 critical_bright = color_blend(PERSIMMON, TANGERINE);
        return color_lerp(critical_base, critical_bright, pulse * 0.7);
    }
}

// Signed distance function for rounded rectangle
float sdRoundedRect(vec2 p, vec2 size, float radius) {
    vec2 d = abs(p) - size + radius;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - radius;
}

void main() {
    // Calculate health color
    vec3 health_color = calculate_health_color(frag_health_perc);
    vec4 frag_fill_color = vec4(health_color, 1.0);

    // Convert UV (0-1) to centered coordinates (-size/2 to size/2)
    vec2 pixel_pos = (frag_uv - 0.5) * frag_size;

    // Fixed roundness value (was always 0.5 from CPU side)
    float frag_roundness = 0.5;

    // Calculate actual corner radius in pixels
    float radius = min(frag_roundness * min(frag_size.x, frag_size.y) * 0.5, min(frag_size.x, frag_size.y) * 0.5);

    // Background rectangle
    float bg_dist = sdRoundedRect(pixel_pos, frag_size * 0.5, radius);

    // Health fill rectangle (clipped to health percentage)
    vec2 fill_size = vec2(frag_size.x * frag_health_perc, frag_size.y) * 0.5;
    vec2 fill_offset = vec2(frag_size.x * (frag_health_perc - 1.0) * 0.5, 0.0);
    float fill_dist = sdRoundedRect(pixel_pos - fill_offset, fill_size, radius);

    // Border
    float border_dist = abs(bg_dist) - u_border_thickness;

    // Smooth anti-aliasing
    float aa_width = 1.0;

    // Calculate alpha masks for each layer
    float bg_mask = 1.0 - smoothstep(-aa_width, aa_width, bg_dist);
    float fill_mask = (frag_health_perc > 0.01) ? (1.0 - smoothstep(-aa_width, aa_width, fill_dist)) : 0.0;
    float border_mask = (1.0 - smoothstep(-aa_width, aa_width, border_dist)) * bg_mask;

    // Discard if completely outside background shape
    if (bg_mask < 0.01) { discard; }

    // Layer composition: background -> fill -> border
    // Use proper alpha blending so all layers fade together
    vec4 out_color = u_bg_color * bg_mask;

    // Fill overwrites background where it exists
    if (frag_health_perc > 0.01) { out_color = mix(out_color, frag_fill_color, fill_mask); }

    // Border is additive on top
    out_color = mix(out_color, u_border_color, border_mask * u_border_color.a);

    color = out_color;
}
