#version 460

// Per-vertex attributes (from base quad)
layout(location = 0) in vec2 vertex_pos;       // Base quad position
layout(location = 1) in vec2 vertex_texcoord;  // Base quad texture coordinates

// Particle data structure matching C++ side
// Total size: 96 bytes for GPU alignment (multiple of 16)
struct Particle {
    vec2 position;         // 8 bytes
    vec2 velocity;         // 8 bytes
    vec4 start_color;      // 16 bytes
    vec4 end_color;        // 16 bytes
    float life;            // 4 bytes
    float initial_life;    // 4 bytes
    float size;            // 4 bytes
    float initial_size;    // 4 bytes
    uint texture_index;    // 4 bytes
    float gravity;         // 4 bytes - per-particle gravity
    float rotation_speed;  // 4 bytes - rotation speed in radians/sec
    float air_resistance;  // 4 bytes - damping factor (0.0 = no damping, 1.0 = full damping)
    uint scene_id;         // 4 bytes - which scene this particle belongs to
    float padding[3];      // 12 bytes - padding to align to 96 bytes (multiple of 16)
};

// Particle buffer
layout(std430, binding = 2) restrict readonly buffer ParticleBuffer {
    Particle particles[];
};

out vec2 tex_coords;
out vec4 particle_color;
flat out uint texture_index;

uniform mat4 u_projection;
uniform uint u_current_scene;

void main() {
    // Get particle data from SSBO using instance ID
    Particle p = particles[gl_InstanceID];

    // Cull dead particles, particles from inactive scenes, and uninitialized particles
    if (p.life <= 0.0 || p.size <= 0.0 || p.scene_id != u_current_scene || (p.position.x == 0.0 && p.position.y == 0.0 && p.initial_life == 0.0)) {
        // Set position to NaN to ensure GPU discards early
        gl_Position = vec4(0.0 / 0.0);  // NaN - guaranteed discard
        return;
    }

    // Use per-particle size
    float scale = p.size;

    // Use per-particle rotation speed
    float rotation = float(gl_InstanceID) * 0.1 + (p.initial_life - p.life) * p.rotation_speed;
    float cos_r = cos(rotation);
    float sin_r = sin(rotation);

    // Rotate the vertex around center
    vec2 rotated_vertex = vec2(vertex_pos.x * cos_r - vertex_pos.y * sin_r, vertex_pos.x * sin_r + vertex_pos.y * cos_r);

    // Transform base quad vertex by particle position and scale
    vec2 world_pos = (rotated_vertex * scale) + p.position;

    // Calculate color interpolation based on lifetime
    float life_perc = p.life / p.initial_life;
    float color_t = smoothstep(0.0, 0.3, 1.0 - life_perc);  // Only transition in final 30%
    vec4 interpolated_color = mix(p.start_color, p.end_color, color_t);

    // Pass through texture coordinates, interpolated color, and texture index
    tex_coords = vertex_texcoord;
    particle_color = interpolated_color;
    texture_index = p.texture_index;

    // Transform to screen space
    gl_Position = u_projection * vec4(world_pos, 0.0, 1.0);
}
