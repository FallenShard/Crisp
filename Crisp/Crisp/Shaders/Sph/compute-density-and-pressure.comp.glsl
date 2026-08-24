#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "sph.part.glsl"

layout(std430, set = 0, binding = 0) buffer Positions {
    vec4 positions[];
};

layout(std430, set = 0, binding = 1) buffer CellCounts {
    uint cellCounts[];
};

layout(std430, set = 0, binding = 2) buffer ReorderedPositions {
    vec4 reorderedPositions[];
};

layout(std430, set = 0, binding = 3) buffer Densities {
    float densities[];
};

layout(std430, set = 0, binding = 4) buffer Pressures {
    float pressures[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

// Must match ParticlePushConstants in Crisp/Crisp/Models/SPH.cpp, field for field. Nothing checks it.
layout(push_constant) uniform PushConstant {
    SphGridParams grid;
    uint numParticles;
}
pc;

const float PI = 3.1415926535897932384626433832795f;
const float particleRadius = 0.01f;
const float particleDiameter = 2.0f * particleRadius;
const float particleVolume = 1.33f * PI * particleRadius * particleRadius * particleRadius;
const float restDensity = 1000.0f;
const float mass = 1.3 * particleVolume * restDensity;
const float stiffness = 100.0f;

const float h = 4.0f * particleRadius;
const float h2 = h * h;
const float h3 = h2 * h;

const float poly6Const = 315.0f / (64.0f * PI * h3 * h3 * h3);

float poly6FromDist2(float dist2) {
    if (dist2 >= h2) {
        return 0.0f;
    }

    float val = h2 - dist2;
    return poly6Const * val * val * val;
}

void main() {
    uint threadIdx = particleGlobalIndex();
    uint numParticles = pc.numParticles;
    if (threadIdx >= numParticles) {
        return;
    }

    vec3 position = positions[threadIdx].xyz;
    ivec3 gridPosition = sphCellUnclamped(position, pc.grid.cellSize);

    ivec3 lo = max(ivec3(0), gridPosition - ivec3(1));
    ivec3 hi = min(ivec3(pc.grid.dim) - ivec3(1), gridPosition + ivec3(1));

    float density = 0.0f;
    for (uint cellZ = lo.z; cellZ <= hi.z; cellZ++) {
        for (uint cellY = lo.y; cellY <= hi.y; cellY++) {
            for (uint cellX = lo.x; cellX <= hi.x; cellX++) {
                uint cellIdx = particleGridLinearIndex(uvec3(cellX, cellY, cellZ), pc.grid.dim);
                uint cellStart = cellCounts[cellIdx];
                uint cellEnd = cellIdx == pc.grid.numCells - 1 ? numParticles : cellCounts[cellIdx + 1];
                for (uint k = cellStart; k < cellEnd; k++) {
                    vec3 diff = position - reorderedPositions[k].xyz;
                    density += mass * poly6FromDist2(dot(diff, diff));
                }
            }
        }
    }

    densities[threadIdx] = density;
    pressures[threadIdx] = max(0.0f, stiffness * (density - restDensity));
}
