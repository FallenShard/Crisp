#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "../Common/pbf.part.glsl"

layout(std430, set = 0, binding = 0) buffer SortedPositions {
    vec4 sortedPositions[];
};

layout(std430, set = 0, binding = 1) buffer DeltaPositions {
    vec4 deltaPositions[];
};

layout(std430, set = 0, binding = 2) buffer SortedIndices {
    uint sortedIndices[];
};

layout(std430, set = 0, binding = 3) buffer Positions {
    vec4 positions[];
};

layout(std430, set = 0, binding = 4) buffer SortedVelocities {
    vec4 sortedVelocities[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(push_constant) uniform PushConstant {
    PbfParams pc;
};

void main() {
    uint k = pbfGlobalIndex();
    if (k >= pc.numParticles) {
        return;
    }

    vec3 corrected = sortedPositions[k].xyz + deltaPositions[k].xyz;
    corrected = clamp(corrected, vec3(pc.particleRadius), pc.spaceSize - vec3(pc.particleRadius));

    uint i = sortedIndices[k];
    sortedVelocities[k] = vec4((corrected - positions[i].xyz) / pc.dt, 0.0f);
    positions[i] = vec4(corrected, 1.0f);
}
