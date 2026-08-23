#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "../Common/pbf.part.glsl"

layout(std430, set = 0, binding = 0) buffer SortedPositions {
    vec4 sortedPositions[];
};

layout(std430, set = 0, binding = 1) buffer CellCounts {
    uint cellCounts[];
};

layout(std430, set = 0, binding = 2) buffer Lambdas {
    float lambdas[];
};

layout(std430, set = 0, binding = 3) buffer DeltaPositions {
    vec4 deltaPositions[];
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

    vec3 position = sortedPositions[k].xyz;
    float lambdaI = lambdas[k];
    ivec3 lo;
    ivec3 hi;
    pbfNeighbourCellRange(position, pc, lo, hi);

    vec3 delta = vec3(0.0f);
    for (int cellZ = lo.z; cellZ <= hi.z; ++cellZ) {
        for (int cellY = lo.y; cellY <= hi.y; ++cellY) {
            for (int cellX = lo.x; cellX <= hi.x; ++cellX) {
                uint cellIdx = pbfGridLinearIndex(uvec3(cellX, cellY, cellZ), pc.gridDim);
                uint cellStart = cellCounts[cellIdx];
                uint cellEnd = cellIdx == pc.numCells - 1u ? pc.numParticles : cellCounts[cellIdx + 1u];
                for (uint m = cellStart; m < cellEnd; ++m) {
                    vec3 diff = position - sortedPositions[m].xyz;
                    float dist2 = dot(diff, diff);
                    if (dist2 <= 0.0f) {
                        continue;
                    }
                    float dist = sqrt(dist2);

                    float ratio = pbfPoly6(dist2, pc.h) / pc.sCorrDenom;
                    float sCorr = -pc.sCorrK * pow(ratio, pc.sCorrN);

                    delta += (lambdaI + lambdas[m] + sCorr) * pbfSpikyGrad(diff, dist, pc.h);
                }
            }
        }
    }

    deltaPositions[k] = vec4(delta * pc.mass / pc.restDensity, 0.0f);
}
