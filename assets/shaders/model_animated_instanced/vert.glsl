#version 330

#define MAX_BONE_NUM 128

// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexBoneIds;
in vec4 vertexBoneWeights;

// Per-instance attributes
in mat4 instanceTransform;
in vec4 instanceTint;  // Per-instance tint color

// Input uniform values
uniform mat4 mvp;                         // View-projection matrix (shared across all instances)
uniform mat4 boneMatrices[MAX_BONE_NUM];  // Shared bone matrices for all instances

// Output vertex attributes (to fragment shader)
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
out vec4 fragInstanceTint;  // Pass instance tint to fragment shader

void main() {
    // GPU Skinning path - apply bone transforms
    int boneIndex0 = int(vertexBoneIds.x);
    int boneIndex1 = int(vertexBoneIds.y);
    int boneIndex2 = int(vertexBoneIds.z);
    int boneIndex3 = int(vertexBoneIds.w);

    vec4 skinnedPosition = vertexBoneWeights.x * (boneMatrices[boneIndex0] * vec4(vertexPosition, 1.0)) +
                           vertexBoneWeights.y * (boneMatrices[boneIndex1] * vec4(vertexPosition, 1.0)) +
                           vertexBoneWeights.z * (boneMatrices[boneIndex2] * vec4(vertexPosition, 1.0)) +
                           vertexBoneWeights.w * (boneMatrices[boneIndex3] * vec4(vertexPosition, 1.0));

    vec4 skinnedNormal = vertexBoneWeights.x * (boneMatrices[boneIndex0] * vec4(vertexNormal, 0.0)) +
                         vertexBoneWeights.y * (boneMatrices[boneIndex1] * vec4(vertexNormal, 0.0)) +
                         vertexBoneWeights.z * (boneMatrices[boneIndex2] * vec4(vertexNormal, 0.0)) +
                         vertexBoneWeights.w * (boneMatrices[boneIndex3] * vec4(vertexNormal, 0.0));

    // Apply instance transform to skinned position
    vec4 worldPosition = instanceTransform * skinnedPosition;

    // Calculate normal matrix for instance transform
    mat3 normalMatrix = mat3(instanceTransform);
    vec3 transformedNormal = normalize(normalMatrix * skinnedNormal.xyz);

    // Send vertex attributes to fragment shader
    fragPosition = worldPosition.xyz;
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = transformedNormal;
    fragInstanceTint = instanceTint;  // Pass instance tint to fragment shader

    // Calculate final vertex position in clip space
    gl_Position = mvp * worldPosition;
}
