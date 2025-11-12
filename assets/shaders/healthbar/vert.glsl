#version 460

// Per-vertex attributes (from base quad)
layout(location = 0) in vec2 vertex_pos;       // Base quad position (0-1 range)
layout(location = 1) in vec2 vertex_texcoord;  // Base quad texture coordinates

// Healthbar instance data structure matching C++ side
// Total size: 32 bytes (multiple of 16 for GPU alignment)
struct HealthbarInstance {
    vec2 screen_pos;        // 8 bytes - screen position (center)
    vec2 bar_size;          // 8 bytes - bar width and height in pixels
    float health_perc;      // 4 bytes - health percentage (0.0 - 1.0)
    float padding[3];       // 12 bytes - padding to align to 32 bytes
};

// Healthbar buffer
layout(std430, binding = 4) restrict readonly buffer HealthbarBuffer {
    HealthbarInstance healthbars[];
};

out vec2 frag_uv;           // 0-1 uv coordinates for the quad
flat out float frag_health_perc;
flat out vec2 frag_size;

uniform mat4 u_view_proj;
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
    vec2 offset = (vertex_pos - 0.5) * hb.bar_size;
    vec2 world_pos = hb.screen_pos + offset;

    // Pass data to fragment shader
    frag_uv = vertex_pos;  // 0-1 range for rounded rectangle calculation
    frag_health_perc = hb.health_perc;
    frag_size = hb.bar_size;

    // Transform to clip space
    gl_Position = u_view_proj * vec4(world_pos, 0.0, 1.0);
}
