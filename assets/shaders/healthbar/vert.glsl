#version 460

// Per-vertex attributes (from base quad)
layout(location = 0) in vec2 vertex_pos;       // Base quad position (0-1 range)
layout(location = 1) in vec2 vertex_texcoord;  // Base quad texture coordinates

// Healthbar instance data structure matching C++ side
// Total size: 32 bytes (multiple of 16 for GPU alignment)
struct HealthbarInstance {
    vec3 world_pos;         // 12 bytes - world position
    float entity_radius;    // 4 bytes - entity radius for size calculation
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
uniform float u_camera_fov;
uniform vec2 u_render_resolution;
uniform uint u_healthbar_count;

// Helper function: project world position to screen space
vec2 worldToScreen(vec3 world_pos) {
    vec4 clip_pos = u_view_proj * vec4(world_pos, 1.0);
    // Perspective divide
    vec3 ndc = clip_pos.xyz / clip_pos.w;
    // NDC to screen space
    vec2 screen_pos;
    screen_pos.x = (ndc.x + 1.0) * 0.5 * u_render_resolution.x;
    screen_pos.y = (1.0 - ndc.y) * 0.5 * u_render_resolution.y;
    return screen_pos;
}

void main() {
    // Get healthbar data from SSBO using instance ID
    if (gl_InstanceID >= u_healthbar_count) {
        // Invalid instance - discard
        gl_Position = vec4(0.0 / 0.0);  // NaN - guaranteed discard
        return;
    }

    HealthbarInstance hb = healthbars[gl_InstanceID];

    // Calculate zoom scale based on FOV (default FOV is 45)
    float zoom_scale = 45.0 / u_camera_fov;

    // Project entity center to screen space
    vec2 center_screen = worldToScreen(hb.world_pos);

    // Project a point at entity radius to calculate screen radius
    vec3 radius_point = hb.world_pos + vec3(hb.entity_radius, 0.0, 0.0);
    vec2 radius_screen = worldToScreen(radius_point);
    float screen_radius = abs(radius_screen.x - center_screen.x);

    // Calculate bar dimensions
    // Width: at most as wide as entity, minimum 40 pixels for visibility
    float bar_width = clamp(screen_radius * 2.0 * 0.8, 40.0, 200.0);
    // Height: scaled by zoom
    float bar_height = 0.5 * zoom_scale;

    vec2 bar_size = vec2(bar_width, bar_height);

    // Offset healthbar above entity (3.5x entity height)
    vec3 healthbar_world_pos = hb.world_pos + vec3(0.0, hb.entity_radius * 3.5, 0.0);
    vec2 screen_pos = worldToScreen(healthbar_world_pos);

    // Transform base quad (0-1) to screen space
    vec2 offset = (vertex_pos - 0.5) * bar_size;
    vec2 final_screen_pos = screen_pos + offset;

    // Pass data to fragment shader
    frag_uv = vertex_pos;  // 0-1 range for rounded rectangle calculation
    frag_health_perc = hb.health_perc;
    frag_size = bar_size;

    // Transform to clip space (orthographic projection for screen space)
    mat4 screen_projection = mat4(
        2.0 / u_render_resolution.x, 0.0, 0.0, 0.0,
        0.0, -2.0 / u_render_resolution.y, 0.0, 0.0,
        0.0, 0.0, -1.0, 0.0,
        -1.0, 1.0, 0.0, 1.0
    );
    gl_Position = screen_projection * vec4(final_screen_pos, 0.0, 1.0);
}
