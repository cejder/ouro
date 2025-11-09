#version 460

// Per-vertex attributes (from base quad)
layout(location = 0) in vec2 vertex_pos;       // Base quad position
layout(location = 1) in vec2 vertex_texcoord;  // Base quad texture coordinates

// Particle data structure matching C++ side
// Total size: 128 bytes for GPU alignment (multiple of 16)
struct Particle {
    vec3 position;         // 12 bytes
    float extra0;          // 4 bytes - extra data slot 0
    vec3 velocity;         // 12 bytes
    float extra1;          // 4 bytes - extra data slot 1
    vec3 acceleration;     // 12 bytes - for custom forces
    float extra2;          // 4 bytes - extra data slot 2
    vec4 start_color;      // 16 bytes
    vec4 end_color;        // 16 bytes
    float life;            // 4 bytes
    float initial_life;    // 4 bytes
    float size;            // 4 bytes
    float initial_size;    // 4 bytes
    uint texture_index;    // 4 bytes
    float gravity;         // 4 bytes - per-particle gravity
    float rotation_speed;  // 4 bytes - rotation speed in radians/sec
    float air_resistance;  // 4 bytes - damping factor
    uint scene_id;         // 4 bytes - which scene this particle belongs to
    uint billboard_mode;   // 4 bytes - 0=camera-facing, 1=velocity-aligned, 2=Y-axis locked, 3=terrain-aligned
    float stretch_factor;  // 4 bytes - velocity-based stretching multiplier
    float extra3;          // 4 bytes - extra data slot 3
};

// Particle buffer
layout(std430, binding = 2) restrict readonly buffer ParticleBuffer {
    Particle particles[];
};

out vec2 tex_coords;
out vec4 particle_color;
flat out uint texture_index;

uniform mat4 u_view_proj;
uniform vec3 u_camera_pos;
uniform vec3 u_camera_right;
uniform vec3 u_camera_up;
uniform uint u_current_scene;

void main() {
    // Get particle data from SSBO using instance ID
    Particle p = particles[gl_InstanceID];

    // Cull dead particles, uninitialized particles, and particles from other scenes
    if (p.life <= 0.0 || p.size <= 0.0 || p.initial_life <= 0.0 || p.scene_id != u_current_scene) {
        // Set position to NaN to ensure GPU discards early
        gl_Position = vec4(0.0 / 0.0);  // NaN - guaranteed discard
        return;
    }

    // Use per-particle size
    float scale = p.size;

    // Calculate billboard orientation based on mode
    vec3 right = u_camera_right;
    vec3 up = u_camera_up;

    // Billboard mode 1: velocity-aligned (stretch along velocity direction)
    if (p.billboard_mode == 1 && length(p.velocity) > 0.01) {
        vec3 vel_dir = normalize(p.velocity);
        right = normalize(cross(vec3(0.0, 1.0, 0.0), vel_dir));
        if (length(right) < 0.01) { right = normalize(cross(vec3(1.0, 0.0, 0.0), vel_dir)); }
        up = vel_dir;

        // Apply stretch factor based on velocity
        float vel_magnitude = length(p.velocity);
        scale *= mix(1.0, p.stretch_factor, clamp(vel_magnitude * 0.1, 0.0, 1.0));
    }
    // Billboard mode 2: Y-axis locked (always upright)
    else if (p.billboard_mode == 2) {
        vec3 to_camera = normalize(u_camera_pos - p.position);
        right = normalize(cross(vec3(0.0, 1.0, 0.0), to_camera));
        up = vec3(0.0, 1.0, 0.0);
    }
    // Billboard mode 3: Terrain-aligned (uses extra0/1/2 as terrain normal)
    else if (p.billboard_mode == 3) {
        vec3 terrain_normal = vec3(p.extra0, p.extra1, p.extra2);
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
    }    // Billboard mode 0: camera-facing (default)
    // Already set above

    // Apply rotation
    float rotation = float(gl_InstanceID) * 0.1 + (p.initial_life - p.life) * p.rotation_speed;
    float cos_r = cos(rotation);
    float sin_r = sin(rotation);

    // Rotate the vertex in billboard space
    vec2 rotated = vec2(vertex_pos.x * cos_r - vertex_pos.y * sin_r, vertex_pos.x * sin_r + vertex_pos.y * cos_r);

    // Build world position from billboard quad
    vec3 world_pos = p.position + (right * rotated.x + up * rotated.y) * scale;

    // Calculate color interpolation based on lifetime
    // life_perc = 1.0 at spawn (top), 0.0 at death (bottom)
    // color_t = 0.0 at spawn (start_color), 1.0 at death (end_color)
    float life_perc = p.life / p.initial_life;
    float color_t = 1.0 - life_perc;  // Linear interpolation over entire lifetime
    vec4 interpolated_color = mix(p.start_color, p.end_color, color_t);

    // Pass through texture coordinates, interpolated color, and texture index
    tex_coords = vertex_texcoord;
    particle_color = interpolated_color;
    texture_index = p.texture_index;

    // Transform to clip space
    gl_Position = u_view_proj * vec4(world_pos, 1.0);
}
