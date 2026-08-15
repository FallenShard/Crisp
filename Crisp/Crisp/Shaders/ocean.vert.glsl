#version 450 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

layout(set = 0, binding = 0) uniform TransformPack {
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

layout(set = 0, binding = 1) uniform sampler2D packedHeightDispXMap;
layout(set = 0, binding = 2) uniform sampler2D packedDispZNormalXMap;
layout(set = 0, binding = 3) uniform sampler2D normalZMap;

layout(push_constant) uniform PushConstant {
    layout(offset = 0) float patchWorldSize;
    layout(offset = 4) int instancesPerSide;
};

layout(location = 0) out vec3 eyePosition;
layout(location = 1) out vec3 eyeNormal;
layout(location = 2) out vec3 worldNormal;

const float g = 9.81;

const float signValue[2] = {1, -1};
float getFactor(int i, int j) {
    return signValue[(i + j) % 2];
}

float getHeight(int i, int j, float factor) {
    return texelFetch(packedHeightDispXMap, ivec2(i, j), 0).r * factor;
}

float getDx(int i, int j, float factor) {
    return texelFetch(packedHeightDispXMap, ivec2(i, j), 0).g * factor;
}

float getDz(int i, int j, float factor) {
    return texelFetch(packedDispZNormalXMap, ivec2(i, j), 0).r * factor;
}

void main() {
    uint patchRow = gl_InstanceIndex / uint(instancesPerSide);
    uint patchCol = gl_InstanceIndex % uint(instancesPerSide);
    float centerOffset = (float(instancesPerSide) - 1.0f) * 0.5f;
    vec3 offset = vec3(
        (float(patchCol) - centerOffset) * patchWorldSize, 0.0f, (float(patchRow) - centerOffset) * patchWorldSize);
    eyeNormal = (N * vec4(normal, 0.0f)).xyz;
    worldNormal = normal;

    eyePosition = (MV * vec4(position + offset, 1.0f)).xyz;
    gl_Position = MVP * vec4(position + offset, 1.0f);
}