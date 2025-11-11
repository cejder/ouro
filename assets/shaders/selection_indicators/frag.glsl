#version 460

in vec2 tex_coords;
in vec4 indicator_color;

out vec4 frag_color;

uniform sampler2D u_texture;

void main() {
    // Sample texture and modulate with indicator color
    vec4 tex_color = texture(u_texture, tex_coords);
    frag_color = tex_color * indicator_color;

    // Discard fully transparent pixels
    if (frag_color.a < 0.01) {
        discard;
    }
}
