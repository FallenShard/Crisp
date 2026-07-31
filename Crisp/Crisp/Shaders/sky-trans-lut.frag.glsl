#version 450 core
#define PI 3.1415926535897932384626433832795

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 finalColor;

#include "Common/atmosphere.part.glsl"

layout(set = 0, binding = 0) uniform AtmosphereParamsBlock {
    AtmosphereParams atmosphere;
};

vec3 integrateOpticalDepth(
    const vec3 worldPos, const vec3 worldDir, in AtmosphereParams atmosphere, in float sampleCount) {
    const float tEnd = intersectAtmosphere(worldPos, worldDir, atmosphere.bottomRadius, atmosphere.topRadius);
    if (tEnd < 0.0f) {
        return vec3(0.0f);
    }

    vec3 opticalDepth = vec3(0.0f);
    float t = 0.0f;
    const float segmentBias = 0.3f;
    for (float s = 0.0f; s < sampleCount; s += 1.0f) {
        // Stepping by the exact difference rather than the nominal step keeps the integration accurate.
        const float tNext = tEnd * (s + segmentBias) / sampleCount;
        const float dt = tNext - t;
        t = tNext;

        const vec3 P = worldPos + t * worldDir;
        const MediumSample mediumSample = sampleMedium(P, atmosphere);
        opticalDepth += mediumSample.extinction * dt;
    }

    return opticalDepth;
}

void main() {
    float viewHeight;
    float viewZenithCosAngle;
    const vec2 uv = vec2(gl_FragCoord.xy) / vec2(kTransmittanceLutWidth, kTransmittanceLutHeight);
    uvToTransmittanceLutParams(atmosphere.bottomRadius, atmosphere.topRadius, uv, viewHeight, viewZenithCosAngle);

    const vec3 worldPos = vec3(0.0f, viewHeight, 0.0f);
    const vec3 worldDir = vec3(0.0f, viewZenithCosAngle, -sqrt(1.0 - viewZenithCosAngle * viewZenithCosAngle));

    const float sampleCount = 40.0f;
    const vec3 transmittance = exp(-integrateOpticalDepth(worldPos, worldDir, atmosphere, sampleCount));
    finalColor = vec4(transmittance, 1.0f);
}