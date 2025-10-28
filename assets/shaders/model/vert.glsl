#version 330

#define MAX_BONE_NUM 128

// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexBoneIds;
in vec4 vertexBoneWeights;

// Input uniform values
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 boneMatrices[MAX_BONE_NUM];
uniform int animationEnabled;

// Output vertex attributes (to fragment shader)
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

// NOTE: Add here your custom variables

void main() {
    vec4 finalPosition;
    vec4 finalNormal;

    if (animationEnabled == 1) {
        // GPU Skinning path
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

        finalPosition = skinnedPosition;
        finalNormal = vec4(normalize(skinnedNormal.xyz), 0.0);
    } else {
        // Regular model path (no skinning)
        finalPosition = vec4(vertexPosition, 1.0);
        finalNormal = vec4(vertexNormal, 0.0);
    }

    // Send vertex attributes to fragment shader
    fragPosition = vec3(matModel * finalPosition);
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = normalize(vec3(matNormal * finalNormal));

    // Calculate final vertex position
    gl_Position = mvp * finalPosition;
}
