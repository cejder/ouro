#version 460
#extension GL_ARB_bindless_texture : require

in vec2 tex_coords;
in vec4 particle_color;
flat in uint texture_index;

out vec4 color;

// Bindless texture handles buffer - allows unlimited textures
layout(std430, binding = 3) restrict readonly buffer TextureHandles {
    sampler2D textures[];
};

void main() {
    // Direct texture access using bindless handles - no limit!
    vec4 tex_color = texture(textures[texture_index], tex_coords);

    // Early discard for completely transparent fragments
    if (tex_color.a * particle_color.a < 0.01) { discard; }

    color = vec4(tex_color.rgb * particle_color.rgb, tex_color.a * particle_color.a);
}
