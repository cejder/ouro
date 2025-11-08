#version 330

// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Per-instance attributes
in mat4 instanceTransform;
in vec4 instanceTint;  // Per-instance tint color

// Input uniform values
uniform mat4 mvp;  // View-projection matrix (shared across all instances)

// Output vertex attributes (to fragment shader)
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
out vec4 fragInstanceTint;  // Pass instance tint to fragment shader

void main() {
    // Apply instance transform to vertex position
    vec4 worldPosition = instanceTransform * vec4(vertexPosition, 1.0);

    // Calculate normal matrix (transpose of inverse of upper 3x3 of instance transform)
    // NOTE: This is simplified - for non-uniform scaling, proper inverse transpose is needed
    mat3 normalMatrix = mat3(instanceTransform);
    vec3 transformedNormal = normalize(normalMatrix * vertexNormal);

    // Send vertex attributes to fragment shader
    fragPosition = worldPosition.xyz;
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = transformedNormal;
    fragInstanceTint = instanceTint;  // Pass instance tint to fragment shader

    // Calculate final vertex position in clip space
    gl_Position = mvp * worldPosition;
}
