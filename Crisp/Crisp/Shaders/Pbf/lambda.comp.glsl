#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "pbf.part.glsl"

layout(std430, set = 0, binding = 0) buffer SortedPositions {
    vec4 sortedPositions[];
};

layout(std430, set = 0, binding = 1) buffer CellCounts {
    uint cellCounts[];
};

layout(std430, set = 0, binding = 2) buffer Densities {
    float densities[];
};

layout(std430, set = 0, binding = 3) buffer Lambdas {
    float lambdas[];
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
    ivec3 lo;
    ivec3 hi;
    pbfNeighbourCellRange(position, pc, lo, hi);

    float density = 0.0f;
    vec3 gradSum = vec3(0.0f);
    float gradSqSum = 0.0f;

    for (int cellZ = lo.z; cellZ <= hi.z; ++cellZ) {
        for (int cellY = lo.y; cellY <= hi.y; ++cellY) {
            for (int cellX = lo.x; cellX <= hi.x; ++cellX) {
                uint cellIdx = particleGridLinearIndex(uvec3(cellX, cellY, cellZ), pc.gridDim);
                uint cellStart = cellCounts[cellIdx];
                uint cellEnd = cellIdx == pc.numCells - 1u ? pc.numParticles : cellCounts[cellIdx + 1u];
                for (uint m = cellStart; m < cellEnd; ++m) {
                    vec3 diff = position - sortedPositions[m].xyz;
                    float dist2 = dot(diff, diff);
                    density += pc.mass * pbfPoly6(dist2, pc.h);

                    vec3 grad = pbfSpikyGrad(diff, sqrt(dist2), pc.h);
                    gradSum += grad;
                    gradSqSum += dot(grad, grad);
                }
            }
        }
    }

    densities[k] = density;

    float invRest = 1.0f / pc.restDensity;
    float constraint = max(density * invRest - 1.0f, 0.0f);
    float scale = pc.mass * invRest;
    float sumGradC2 = scale * scale * (dot(gradSum, gradSum) + gradSqSum);

    lambdas[k] = -constraint / max(sumGradC2 + pc.alphaTilde, 1e-12f);
}
