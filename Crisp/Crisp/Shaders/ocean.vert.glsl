#version 450 core

#extension GL_GOOGLE_include_directive : require

#include "Common/ocean-draw.part.glsl"

layout(location = 0) in vec3 position;

layout(set = 0, binding = 0) uniform TransformPack {
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

layout(set = 0, binding = 1) uniform sampler2DArray packedHeightDispXMap;
layout(set = 0, binding = 2) uniform sampler2DArray packedDispZNormalXMap;

layout(location = 0) out vec3 eyePosition;
layout(location = 1) out vec2 oceanWorldXZ;

void main() {
    const uint patchRow = gl_InstanceIndex / uint(instancesPerSide);
    const uint patchCol = gl_InstanceIndex % uint(instancesPerSide);
    const float centerOffset = (float(instancesPerSide) - 1.0f) * 0.5f;
    const vec3 offset = vec3(
        (float(patchCol) - centerOffset) * patchWorldSize, 0.0f, (float(patchRow) - centerOffset) * patchWorldSize);

    // Sampling by position rather than by patch-local grid index is what lets each cascade keep its
    // own period across instances; indexing by grid coordinate would repeat all of them at
    // patchWorldSize and undo the point of picking near-prime patch sizes.
    const vec3 basePos = position + offset;
    oceanWorldXZ = basePos.xz;

    const float fftSize = float(textureSize(packedHeightDispXMap, 0).x);
    const float vertexSpacing = patchWorldSize / float(gridSize);

    vec3 displaced = basePos;
    for (int c = 0; c < OCEAN_CASCADE_COUNT; ++c) {
        const vec3 uv = oceanCascadeUv(oceanWorldXZ, cascadeSizes[c], fftSize, c);
        // A band the vertex grid cannot resolve would land as per-vertex noise, not as waves.
        const float weight = oceanBandResolveWeight(cascadeWavelengths[c], vertexSpacing);
        if (weight <= 0.0f) {
            continue;
        }

        const vec2 heightDispX = textureLod(packedHeightDispXMap, uv, 0.0f).rg;
        const float dispZ = textureLod(packedDispZNormalXMap, uv, 0.0f).r;
        // Tessendorf choppy waves; the spectrum's D(k) = -i*(k/|k|)*h~ makes the scale negative.
        displaced += weight * vec3(-choppiness * heightDispX.g, heightDispX.r, -choppiness * dispZ);
    }

    eyePosition = (MV * vec4(displaced, 1.0f)).xyz;
    gl_Position = MVP * vec4(displaced, 1.0f);
}
