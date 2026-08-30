#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "Common/math-constants.part.glsl"
#include "PathTracer/Lights/environment-distribution.part.glsl"

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

struct SamplingResult {
    vec4 directionAndPdf;
    uvec4 texel;
};

layout(set = 0, binding = 0, std430) writeonly buffer Results {
    SamplingResult results[];
};

layout(push_constant, scalar) uniform PushConstants {
    EnvironmentCdf distribution;
    uint width;
    uint height;
    uint sampleCount;
    uint pad0;
};

uint hashUint(uint value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float unitFloat(uint value) {
    return (float(hashUint(value)) + 0.5f) * (1.0f / 4294967296.0f);
}

void main() {
    const uint index = gl_GlobalInvocationID.x;
    if (index >= sampleCount) {
        return;
    }

    vec2 uv;
    float pdf;
    const vec3 direction = sampleEnvironmentDirection(
        distribution,
        width,
        height,
        vec2(unitFloat(2u * index + 0x68bc21ebu), unitFloat(2u * index + 0x967a889bu)),
        uv,
        pdf);
    results[index].directionAndPdf = vec4(direction, pdf);
    results[index].texel = uvec4(
        min(uint(uv.x * float(width)), width - 1u),
        min(uint(uv.y * float(height)), height - 1u),
        0u,
        0u);
}
