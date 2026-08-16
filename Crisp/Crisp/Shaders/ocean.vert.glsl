#version 450 core

layout(location = 0) in vec3 position;

layout(set = 0, binding = 0) uniform TransformPack {
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

layout(push_constant) uniform PushConstant {
    layout(offset = 0) vec3 sunDirection;
    layout(offset = 12) float sunIntensity;
    layout(offset = 16) float patchWorldSize;
    layout(offset = 20) int instancesPerSide;
    layout(offset = 24) int gridSize;
    layout(offset = 28) float choppiness;
    layout(offset = 32) float waterRoughness;
    layout(offset = 36) float foamThreshold;
    layout(offset = 40) float foamSoftness;
    layout(offset = 44) float foamIntensity;
    layout(offset = 48) float invRmsWaveHeight;
};

layout(location = 0) out vec3 eyePosition;
layout(location = 1) out vec2 oceanUv;

void main() {
    const uint patchRow = gl_InstanceIndex / uint(instancesPerSide);
    const uint patchCol = gl_InstanceIndex % uint(instancesPerSide);
    const float centerOffset = (float(instancesPerSide) - 1.0f) * 0.5f;
    const vec3 offset = vec3(
        (float(patchCol) - centerOffset) * patchWorldSize, 0.0f, (float(patchRow) - centerOffset) * patchWorldSize);

    const uint gridWidth = uint(gridSize + 1);
    const uvec2 gridCoord = uvec2(uint(gl_VertexIndex) % gridWidth, uint(gl_VertexIndex) / gridWidth);

    // Texel centres, not corners: the geometry pass displaces vertex (i, j) by texelFetch(i, j).
    oceanUv = (vec2(gridCoord) + 0.5f) / float(gridSize);

    eyePosition = (MV * vec4(position + offset, 1.0f)).xyz;
    gl_Position = MVP * vec4(position + offset, 1.0f);
}
