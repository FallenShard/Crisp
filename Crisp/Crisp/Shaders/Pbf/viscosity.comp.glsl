#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "pbf.part.glsl"

layout(std430, set = 0, binding = 0) buffer SortedPositions {
    vec4 sortedPositions[];
};

layout(std430, set = 0, binding = 1) buffer CellCounts {
    uint cellCounts[];
};

layout(std430, set = 0, binding = 2) buffer SortedVelocities {
    vec4 sortedVelocities[];
};

layout(std430, set = 0, binding = 3) buffer Densities {
    float densities[];
};

layout(std430, set = 0, binding = 4) buffer SortedIndices {
    uint sortedIndices[];
};

layout(std430, set = 0, binding = 5) buffer Velocities {
    vec4 velocities[];
};

layout(std430, set = 0, binding = 6) buffer Colors {
    vec4 colors[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(push_constant) uniform PushConstant {
    PbfParams pc;
};

void main() {
    uint k = particleGlobalIndex();
    if (k >= pc.numParticles) {
        return;
    }

    vec3 position = sortedPositions[k].xyz;
    vec3 velocity = sortedVelocities[k].xyz;
    ivec3 lo;
    ivec3 hi;
    pbfNeighbourCellRange(position, pc, lo, hi);

    vec3 accum = vec3(0.0f);
    for (int cellZ = lo.z; cellZ <= hi.z; ++cellZ) {
        for (int cellY = lo.y; cellY <= hi.y; ++cellY) {
            for (int cellX = lo.x; cellX <= hi.x; ++cellX) {
                uint cellIdx = particleGridLinearIndex(uvec3(cellX, cellY, cellZ), pc.gridDim);
                uint cellStart = cellCounts[cellIdx];
                uint cellEnd = cellIdx == pc.numCells - 1u ? pc.numParticles : cellCounts[cellIdx + 1u];
                for (uint m = cellStart; m < cellEnd; ++m) {
                    vec3 diff = position - sortedPositions[m].xyz;
                    float weight = pbfPoly6(dot(diff, diff), pc.h);
                    if (weight <= 0.0f) {
                        continue;
                    }
                    accum += (sortedVelocities[m].xyz - velocity) * weight * pc.mass / max(densities[m], 1e-6f);
                }
            }
        }
    }

    vec3 smoothed = velocity + pc.xsphC * accum;

    uint i = sortedIndices[k];
    velocities[i] = vec4(smoothed, 0.0f);

    float t = clamp(length(smoothed) * pc.colorSpeedScale, 0.0f, 1.0f);
    colors[i] = vec4(mix(vec3(0.05f, 0.15f, 0.45f), vec3(0.55f, 0.8f, 1.0f), t), 1.0f);
}
