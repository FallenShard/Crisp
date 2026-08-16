#ifndef CRISP_OCEAN_GLSL
#define CRISP_OCEAN_GLSL

// Fixed at compile time so the sampling loops stay in uniform control flow.
// Must match kOceanCascadeCount in Crisp/Models/Ocean.hpp.
#define OCEAN_CASCADE_COUNT 3

// Cascade texel (i, j) sits at world (i * L / N - L / 2), so the world origin lands on texel N / 2.
// The half texel keeps the tap on the texel centre rather than its corner.
vec3 oceanCascadeUv(const vec2 worldXZ, const float patchWorldSize, const float fftSize, const int cascade) {
    return vec3(worldXZ / patchWorldSize + 0.5f + 0.5f / fftSize, float(cascade));
}

// A band is only worth sampling while its waves stay wider than two samples; below that the tap is
// noise rather than signal, and the energy belongs in the specular lobe instead. Both the vertex
// grid and the pixel footprint fade their bands out through here, which is what keeps the geometry
// and the shading agreeing on which cascades are present. See docs/ocean.md item 12.
float oceanBandResolveWeight(const float bandWavelength, const float sampleSpacing) {
    return smoothstep(1.0f, 3.0f, bandWavelength / max(sampleSpacing, 1e-5f));
}

#endif // CRISP_OCEAN_GLSL
