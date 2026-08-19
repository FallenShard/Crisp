#ifndef CRISP_OCEAN_DRAW_GLSL
#define CRISP_OCEAN_DRAW_GLSL

#include "ocean.part.glsl"

// Shared by ocean.vert and ocean.frag. Both stages reconstruct the same cascade weights from it, and
// a mismatch would shade geometry that was displaced by a different set of bands, so the block lives
// here rather than being spelled out twice. Mirrors OceanPushConstants in Scenes/OceanScene.cpp.
layout(push_constant) uniform OceanPushConstant {
    // The mesh patch spans cascade 0; the finer cascades tile inside it at their own periods.
    layout(offset = 0) float patchWorldSize;
    layout(offset = 4) int instancesPerSide;
    layout(offset = 8) int gridSize;
    layout(offset = 12) float choppiness;

    layout(offset = 16) float waterRoughness;
    layout(offset = 20) float foamThreshold;
    layout(offset = 24) float foamSoftness;
    layout(offset = 28) float foamIntensity;
    layout(offset = 32) float invRmsWaveHeight;
    layout(offset = 36) float slopeVarianceScale;
    layout(offset = 40) float foamPatchWorldSize;
    layout(offset = 44) int foamLayer;

    layout(offset = 48) vec4 cascadeSizes;
    // Geometric mean of each band's wavelength range, against which a sample spacing is judged.
    layout(offset = 64) vec4 cascadeWavelengths;
    // Per-axis slope variance of each band, folded into the specular lobe once the band is lost.
    layout(offset = 80) vec4 cascadeSlopeVariances;

    layout(offset = 96) float foamErosion;
    layout(offset = 100) float foamFreshness;
};

#endif // CRISP_OCEAN_DRAW_GLSL
