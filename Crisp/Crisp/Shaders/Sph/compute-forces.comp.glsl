#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "sph.part.glsl"

layout(std430, set = 0, binding = 0) buffer Positions {
    vec4 positions[];
};

layout(std430, set = 0, binding = 1) buffer CellCounts {
    uint cellCounts[];
};

// Particles belonging to cell i are identified with
// indices[cellCounts[i]]...indices[cellCounts[i + 1]]
layout(std430, set = 0, binding = 2) buffer Indices {
    uint indices[];
};

layout(std430, set = 0, binding = 3) buffer Densities {
    float densities[];
};

layout(std430, set = 0, binding = 4) buffer Pressures {
    float pressures[];
};

layout(std430, set = 0, binding = 5) buffer Velocities {
    vec4 velocities[];
};

layout(std430, set = 0, binding = 6) buffer Forces {
    vec4 forces[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

// Must match ForcesPushConstants in Crisp/Crisp/Models/SPH.cpp, field for field. Nothing checks it.
layout(push_constant) uniform PushConstant {
    SphGridParams grid;
    vec3 gravity;
    uint numParticles;
    float viscosity;
    float kappa;
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

const float spikyGradConst = -45.0f / (PI * h2 * h2 * h2);
const float viscosityLaplaceConst = 45.0f / (PI * h3 * h3);

float cubicSpline(float x) {
    if (x >= h) {
        return 0.0f;
    }

    float q = x / h;
    const float c = 3.0f / (2.0f * PI);
    if (q > 0.5f) {
        float a = (2 - q);
        return 1.0f / 6.0f * a * a * a;
    } else {
        float q2 = q * q;
        return 2.0f / 3.0f - q2 + 0.5f * q2 * q;
    }
}

vec3 spikyGrad(vec3 v, float x) {
    return spikyGradConst * (h - x) * (h - x) * (1.0f / x) * v;
}

float viscoLaplacian(float x) {
    return viscosityLaplaceConst * (h - x);
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

    vec3 velI = velocities[threadIdx].xyz;
    float pressureI = pressures[threadIdx];
    float densityI = densities[threadIdx];

    vec3 fPressure = vec3(0.0f);
    vec3 fViscosity = vec3(0.0f);
    vec3 fTension = vec3(0.0f);
    for (uint cellZ = lo.z; cellZ <= hi.z; cellZ++) {
        for (uint cellY = lo.y; cellY <= hi.y; cellY++) {
            for (uint cellX = lo.x; cellX <= hi.x; cellX++) {
                uint cellIdx = particleGridLinearIndex(uvec3(cellX, cellY, cellZ), pc.grid.dim);
                uint cellStart = cellCounts[cellIdx];
                uint cellEnd = cellIdx == pc.grid.numCells - 1 ? pc.numParticles : cellCounts[cellIdx + 1];
                for (uint k = cellStart; k < cellEnd; k++) {
                    uint j = indices[k];
                    vec3 posJ = positions[j].xyz;

                    vec3 diff = position - posJ;
                    float dist2 = dot(diff, diff);

                    if (dist2 > 0.0f && dist2 < h2) {
                        float volJ = mass / densities[j];
                        float dist = sqrt(dist2);

                        fPressure += -volJ * (pressureI + pressures[j]) * 0.5f * spikyGrad(diff, dist);
                        fViscosity += +volJ * (velocities[j].xyz - velI) * viscoLaplacian(dist);

                        fTension += -cubicSpline(dist) * diff / dist;
                    }
                }
            }
        }
    }

    vec3 fGravity = densityI * pc.gravity;
    vec3 fTotal = fPressure + pc.viscosity * fViscosity + fGravity + densityI * pc.kappa * fTension;

    forces[threadIdx] = vec4(fTotal, densityI);
}
