#version 330

uniform mat4 mvp;
uniform float minHeight;
uniform float maxHeight;
in vec3 vertexPosition;
in mat4 instanceTransform;
out float normalizedHeight;

void main() {
    vec4 worldPos = instanceTransform * vec4(vertexPosition, 1.0);

    // Normalize height in vertex shader (much faster than per-fragment)
    normalizedHeight = clamp((worldPos.y - minHeight) / (maxHeight - minHeight), 0.0, 1.0);

    gl_Position = mvp * worldPos;
}
