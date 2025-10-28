#version 460 core
layout(location = 0) in vec3 a_pos;

out vec3 v_world_pos;

uniform mat4 u_projection;
uniform mat4 u_view;

void main() {
    v_world_pos = a_pos;
    gl_Position = u_projection * u_view * vec4(a_pos, 1.0);
}
