#ifndef PBF_PART_GLSL
#define PBF_PART_GLSL

#include "../Common/particle-grid.part.glsl"

// Shared by every pass of the position-based fluid solver. Nothing here hardcodes a particle size:
// the solver's scale lives in PbfParams, which C++ owns and pushes, so changing PbfConfig cannot
// silently disagree with the shaders.

const float kPi = 3.1415926535897932384626433832795f;

// Must match PbfPushConstants in Crisp/Crisp/Models/Pbf.cpp, field for field. Nothing checks it.
struct PbfParams {
    uvec3 gridDim;
    uint numCells;

    vec3 spaceSize;
    float cellSize;

    vec3 gravity;
    // Kernel support radius. The grid cell is this wide, which is what makes a 3x3x3 cell sweep
    // cover the whole support.
    float h;

    float mass;
    float restDensity;
    uint numParticles;
    float dt;

    // XPBD compliance divided by dt^2. Softens the density constraint by an amount that does not
    // move when the substep does -- the whole reason this is XPBD rather than PBF's tuned epsilon.
    float alphaTilde;
    // Artificial pressure (Macklin & Muller s_corr): strength, exponent, and the precomputed
    // W(sCorrDq) it is measured against.
    float sCorrK;
    float sCorrN;
    float sCorrDenom;

    float xsphC; // XSPH viscosity coefficient.
    float particleRadius;
    float colorSpeedScale;
    float _pad;
};

// Poly6, in terms of the squared distance: the kernel never needs the root, and most of what a
// 3x3x3 cell sweep produces falls outside the support anyway.
float pbfPoly6(float dist2, float h) {
    float h2 = h * h;
    if (dist2 >= h2) {
        return 0.0f;
    }
    float val = h2 - dist2;
    float h3 = h2 * h;
    return 315.0f / (64.0f * kPi * h3 * h3 * h3) * val * val * val;
}

// Spiky gradient. Poly6's gradient vanishes as r -> 0, which is exactly what lets particles clump,
// so anything differentiated uses this instead.
vec3 pbfSpikyGrad(vec3 diff, float dist, float h) {
    if (dist >= h || dist <= 0.0f) {
        return vec3(0.0f);
    }
    float h2 = h * h;
    float coeff = -45.0f / (kPi * h2 * h2 * h2) * (h - dist) * (h - dist);
    return coeff * diff / dist;
}

// The 3x3x3 block of cells covering one particle's kernel support.
void pbfNeighbourCellRange(vec3 position, PbfParams pc, out ivec3 lo, out ivec3 hi) {
    ivec3 gridPosition = ivec3(particleGridPosition(position, pc.cellSize, pc.gridDim));
    lo = max(ivec3(0), gridPosition - ivec3(1));
    hi = min(ivec3(pc.gridDim) - ivec3(1), gridPosition + ivec3(1));
}

#endif // PBF_PART_GLSL
