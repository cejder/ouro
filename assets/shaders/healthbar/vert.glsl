#version 460

// Per-vertex attributes (from base quad)
layout(location = 0) in vec2 vertex_pos;       // Base quad position (0-1 range)
layout(location = 1) in vec2 vertex_texcoord;  // Base quad texture coordinates

// Healthbar instance data structure matching C++ side
// Total size: 48 bytes (multiple of 16 for GPU alignment)
struct HealthbarInstance {
    vec2 screen_pos;        // 8 bytes - center position on screen
    vec2 size;              // 8 bytes - width and height
    vec4 fill_color;        // 16 bytes - health bar fill color
    float health_perc;      // 4 bytes - health percentage (0.0 - 1.0)
    float roundness;        // 4 bytes - corner roundness
    uint is_multi_select;   // 4 bytes - 0 = single select (complex), 1 = multi select (minimal)
    uint padding;           // 4 bytes - padding to align to 48 bytes
};

// Healthbar buffer
layout(std430, binding = 4) restrict readonly buffer HealthbarBuffer {
    HealthbarInstance healthbars[];
};

out vec2 frag_uv;           // 0-1 uv coordinates for the quad
out vec4 frag_fill_color;
flat out float frag_health_perc;
flat out float frag_roundness;
flat out vec2 frag_size;

uniform mat4 u_projection;
uniform uint u_healthbar_count;

void main() {
    // Get healthbar data from SSBO using instance ID
    if (gl_InstanceID >= u_healthbar_count) {
        // Invalid instance - discard
        gl_Position = vec4(0.0 / 0.0);  // NaN - guaranteed discard
        return;
    }

    HealthbarInstance hb = healthbars[gl_InstanceID];

    // Transform base quad (0-1) to screen space
    // Center the quad around screen_pos
    vec2 offset = (vertex_pos - 0.5) * hb.size;
    vec2 world_pos = hb.screen_pos + offset;

    // Pass data to fragment shader
    frag_uv = vertex_pos;  // 0-1 range for rounded rectangle calculation
    frag_fill_color = hb.fill_color;
    frag_health_perc = hb.health_perc;
    frag_roundness = hb.roundness;
    frag_size = hb.size;

    // Transform to clip space
    gl_Position = u_projection * vec4(world_pos, 0.0, 1.0);
}
