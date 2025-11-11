#version 460

// Per-vertex attributes (from base quad)
layout(location = 0) in vec2 vertex_pos;       // Base quad position
layout(location = 1) in vec2 vertex_texcoord;  // Base quad texture coordinates

// Instance data structure matching C++ side
// Total size: 64 bytes for GPU alignment
struct IndicatorInstance {
    vec3 position;       // 12 bytes
    float rotation;      // 4 bytes
    float size;          // 4 bytes
    float terrain_normal_x;   // 4 bytes
    float terrain_normal_y;   // 4 bytes
    float terrain_normal_z;   // 4 bytes
    vec4 color;          // 16 bytes
    vec2 padding;        // 8 bytes
};

// Instance buffer
layout(std430, binding = 2) restrict readonly buffer InstanceBuffer {
    IndicatorInstance instances[];
};

out vec2 tex_coords;
out vec4 indicator_color;

uniform mat4 u_view_proj;
uniform vec3 u_camera_pos;
uniform vec3 u_camera_right;
uniform vec3 u_camera_up;

void main() {
    // Get instance data from SSBO using instance ID
    IndicatorInstance inst = instances[gl_InstanceID];

    // Use instance size
    float scale = inst.size;

    // Calculate billboard orientation (terrain-aligned mode)
    vec3 terrain_normal = vec3(inst.terrain_normal_x, inst.terrain_normal_y, inst.terrain_normal_z);
    vec3 right, up;

    float len = length(terrain_normal);
    if (len < 0.01) {
        // Default to flat horizontal if no normal provided
        right = vec3(1.0, 0.0, 0.0);
        up = vec3(0.0, 0.0, 1.0);
    } else {
        // Orient billboard tangent to terrain (flat on the surface)
        terrain_normal = normalize(terrain_normal);

        // Create tangent vectors perpendicular to terrain normal
        vec3 world_up = vec3(0.0, 1.0, 0.0);
        right = cross(world_up, terrain_normal);

        // Handle case where normal is aligned with world up
        if (length(right) < 0.01) {
            world_up = vec3(1.0, 0.0, 0.0);
            right = cross(world_up, terrain_normal);
        }

        right = normalize(right);
        up = normalize(cross(terrain_normal, right));
    }

    // Apply rotation
    float rotation = inst.rotation;
    float cos_r = cos(rotation);
    float sin_r = sin(rotation);

    // Rotate the vertex in billboard space
    vec2 rotated = vec2(vertex_pos.x * cos_r - vertex_pos.y * sin_r, vertex_pos.x * sin_r + vertex_pos.y * cos_r);

    // Build world position from billboard quad
    vec3 world_pos = inst.position + (right * rotated.x + up * rotated.y) * scale;

    // Pass through texture coordinates and color
    tex_coords = vertex_texcoord;
    indicator_color = inst.color;

    // Transform to clip space
    gl_Position = u_view_proj * vec4(world_pos, 1.0);
}
