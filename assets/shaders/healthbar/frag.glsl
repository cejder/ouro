#version 460

in vec2 frag_uv;
in vec4 frag_fill_color;
flat in float frag_health_perc;
flat in float frag_roundness;
flat in vec2 frag_size;

out vec4 color;

uniform vec4 u_bg_color;
uniform vec4 u_border_color;
uniform float u_border_thickness;

// Signed distance function for rounded rectangle
float sdRoundedRect(vec2 p, vec2 size, float radius) {
    vec2 d = abs(p) - size + radius;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - radius;
}

void main() {
    // Convert UV (0-1) to centered coordinates (-size/2 to size/2)
    vec2 pixel_pos = (frag_uv - 0.5) * frag_size;

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

    // Layer composition (back to front):
    vec4 out_color = vec4(0.0);

    // 1. Background
    float bg_alpha = 1.0 - smoothstep(-aa_width, aa_width, bg_dist);
    out_color = mix(out_color, u_bg_color, bg_alpha);

    // 2. Health fill (only where health > 0)
    if (frag_health_perc > 0.01) {
        float fill_alpha = 1.0 - smoothstep(-aa_width, aa_width, fill_dist);
        out_color = mix(out_color, frag_fill_color, fill_alpha);
    }

    // 3. Border (on top)
    float border_alpha = (1.0 - smoothstep(-aa_width, aa_width, border_dist)) * (1.0 - smoothstep(-aa_width, aa_width, bg_dist));
    out_color = mix(out_color, u_border_color, border_alpha);

    // Discard fully transparent pixels
    if (out_color.a < 0.01) {
        discard;
    }

    color = out_color;
}
