#version 460

in vec2 tex_coords;
in vec4 indicator_color;

out vec4 frag_color;

void main() {
    // Convert tex coords from [0,1] to [-1,1] (centered)
    vec2 uv = tex_coords * 2.0 - 1.0;
    float dist = length(uv);

    // Create a circular ring pattern
    float outer_radius = 0.95;
    float inner_radius = 0.70;
    float ring_thickness = outer_radius - inner_radius;

    // Main ring with smooth edges
    float ring = smoothstep(inner_radius - 0.1, inner_radius, dist) *
                 smoothstep(outer_radius + 0.1, outer_radius, dist);

    // Add subtle inner glow
    float glow = exp(-dist * dist * 2.0) * 0.3;

    // Combine ring and glow
    float alpha = clamp(ring + glow, 0.0, 1.0);

    // Apply color
    frag_color = vec4(indicator_color.rgb, indicator_color.a * alpha);

    // Discard fully transparent pixels
    if (frag_color.a < 0.01) {
        discard;
    }
}
