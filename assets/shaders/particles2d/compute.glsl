#version 450

// Local work group size - 64 threads per work group is common
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

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

// Shader Storage Buffer Object (SSBO) for particle data
layout(std430, binding = 0) restrict buffer ParticleBuffer {
    Particle particles[];
};

// Atomic counter for active particle count
layout(std430, binding = 1) restrict buffer CounterBuffer {
    uint active_count;
};

// Uniforms for simulation parameters
uniform float u_delta_time;
uniform float u_time;
uniform vec2 u_resolution;
uniform uint u_particle_count;
uniform uint u_current_scene;

// Simple random function for GPU
float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    // Get current particle index
    uint index = gl_GlobalInvocationID.x;

    // Early exit if we're beyond particle count
    if (index >= u_particle_count) { return; }

    // Get reference to current particle
    Particle p = particles[index];

    // Skip particles that don't belong to the current scene or are uninitialized
    if (p.scene_id != u_current_scene || p.initial_life <= 0.0) { return; }

    // Update particle life
    p.life -= u_delta_time;

    if (p.life > 0.0) {
        // Apply per-particle gravity
        p.velocity.y += p.gravity * u_delta_time;

        // Apply per-particle air resistance - frame rate independent
        float damping = 1.0 - p.air_resistance;
        p.velocity *= pow(damping, u_delta_time * 60.0);

        // Update position based on velocity
        p.position += p.velocity * u_delta_time;

        // Calculate life percentage (1.0 = full life, 0.0 = dead)
        float life_perc = p.life / p.initial_life;

        // Smooth fade out with ease-out cubic
        float ease_t = 1.0 - life_perc;
        float alpha = 1.0 - (ease_t * ease_t * ease_t);

        // Size animation - smooth fade from initial size to smaller over lifetime
        float size_mult = mix(1.0, 0.1, 1.0 - life_perc);
        p.size = p.initial_size * size_mult;

        // Don't modify the original colors - just update alpha for fading
        p.start_color.a = alpha;
        p.end_color.a = alpha;

        particles[index] = p;
    } else {
        // Mark particle as dead
        p.life = -1.0;
        p.size = 0.0;
        p.start_color.a = 0.0;
        p.end_color.a = 0.0;
        particles[index] = p;
    }
}
