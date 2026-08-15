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

// From the original paper "PCG: A Family of Simple Fast Space-Efficient Statistically Good Algorithms for Random Number Generation", 2014.
// https://www.cs.hmc.edu/tr/hmc-cs-2014-0905.pdf
uint pcg(inout uint state) {
    state = state * 747796405u + 2891336453u;
    const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

uint seedRng(uvec2 pixel, uint frameIdx) {
    return pcg3d(uvec3(pixel, frameIdx)).x;
}

float rndFloat(inout uint seed) {
    return float(pcg(seed) >> 8) * (1.0 / 16777216.0);
}

uint rndRange(inout uint seed, uint upper) {
    const float f = rndFloat(seed);
    return clamp(uint(f * float(upper)), 0, upper - 1);
}

#endif // CRISP_RNG_GLSL
