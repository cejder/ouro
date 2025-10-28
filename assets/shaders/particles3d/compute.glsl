#version 450

// Local work group size - 64 threads per work group is common
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

// Particle data structure matching C++ side
// Total size: 128 bytes for GPU alignment (multiple of 16)
struct Particle {
    vec3 position;         // 12 bytes
    float padding0;        // 4 bytes - align to 16
    vec3 velocity;         // 12 bytes
    float padding1;        // 4 bytes - align to 16
    vec3 acceleration;     // 12 bytes - for custom forces
    float padding2;        // 4 bytes - align to 16
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
    uint billboard_mode;   // 4 bytes - 0=camera-facing, 1=velocity-aligned, 2=Y-axis locked
    float stretch_factor;  // 4 bytes - velocity-based stretching multiplier
    float padding3;        // 4 bytes - padding
};

// Shader Storage Buffer Object (SSBO) for particle data
layout(std430, binding = 0) restrict buffer ParticleBuffer {
    Particle particles[];
};

// Uniforms for simulation parameters
uniform float u_delta_time;
uniform float u_time;
uniform uint u_particle_count;
uniform uint u_current_scene;

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
        // Apply custom acceleration (wind, explosions, etc.)
        p.velocity += p.acceleration * u_delta_time;

        // Apply per-particle gravity (down is negative Y)
        p.velocity.y -= p.gravity * u_delta_time;

        // Apply per-particle air resistance - frame rate independent
        float damping = 1.0 - p.air_resistance;
        p.velocity *= pow(damping, u_delta_time * 60.0);

        // Update position based on velocity
        p.position += p.velocity * u_delta_time;

        // Calculate life percentage (1.0 = full life, 0.0 = dead)
        float life_perc = p.life / p.initial_life;

        // Size and alpha animation - controlled by stretch_factor
        // If stretch_factor is 0.0: pure interpolation mode (size static, alpha interpolates in vertex shader)
        // If stretch_factor > 0.0: animated mode (size shrinks, alpha fades with cubic easing)
        if (p.stretch_factor > 0.0) {
            // Animated particles: shrink size over lifetime
            float size_mult = mix(1.0, 0.1, 1.0 - life_perc);
            p.size = p.initial_size * size_mult;

            // Apply cubic alpha fade for smooth fade-out
            float ease_t = 1.0 - life_perc;
            float alpha = 1.0 - (ease_t * ease_t * ease_t);
            p.start_color.a = alpha;
            p.end_color.a = alpha;
        } else {
            // Pure interpolation mode: keep size constant, let vertex shader handle alpha interpolation
            p.size = p.initial_size;
            // Don't touch start_color.a or end_color.a - they interpolate in vertex shader
        }

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
