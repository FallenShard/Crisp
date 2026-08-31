#ifndef CRISP_RNG_GLSL
#define CRISP_RNG_GLSL

// Jarzynski and Olano, "Hash Functions for GPU Rendering", 2020.
// https://jcgt.org/published/0009/03/02/ for reference.
uvec3 pcg3d(uvec3 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v ^= v >> 16u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    return v;
}

float uintToUnitFloat(uint u) {
    return float(u >> 8) * (1.0 / 16777216.0);
}

struct Sampler {
    uint seed;      // Per-pixel decorrelation.
    uint sampleIdx; // frameIdx * sampleCount + i, unique per accumulated sample.
    uint dimension; // Cursor into the sample vector, reset per bounce rather than free-running.
};

Sampler createSampler(uvec2 pixel, uint sampleIdx, uint sceneSeed) {
    Sampler s;
    s.seed = pcg3d(uvec3(pixel, sceneSeed)).x;
    s.sampleIdx = sampleIdx;
    s.dimension = 0u;
    return s;
}

Sampler createSampler(uvec2 pixel, uint sampleIdx) {
    return createSampler(pixel, sampleIdx, 0u);
}

// Paths of different lengths must not drift against each other, so callers restart the cursor at a
// fixed base per bounce instead of letting it run on. See the kDim* constants in Core/types.
void setDimension(inout Sampler s, uint dimension) {
    s.dimension = dimension;
}

float next1D(inout Sampler s) {
    return uintToUnitFloat(pcg3d(uvec3(s.seed, s.sampleIdx, s.dimension++)).x);
}

// pcg3d's outputs are independent, so one hash covers a 2D draw. The cursor still advances by two,
// keeping the dimension budget identical to a backend that resolves each dimension separately.
vec2 next2D(inout Sampler s) {
    const uvec3 h = pcg3d(uvec3(s.seed, s.sampleIdx, s.dimension));
    s.dimension += 2u;
    return vec2(uintToUnitFloat(h.x), uintToUnitFloat(h.y));
}

uint nextRange(inout Sampler s, uint upper) {
    return min(uint(next1D(s) * float(upper)), upper - 1u);
}

#endif // CRISP_RNG_GLSL
