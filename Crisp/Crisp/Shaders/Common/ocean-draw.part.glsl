#ifndef CRISP_OCEAN_DRAW_GLSL
#define CRISP_OCEAN_DRAW_GLSL

#include "ocean.part.glsl"

// Shared by ocean.vert and ocean.frag. Both stages reconstruct the same cascade weights from it, and
// a mismatch would shade geometry that was displaced by a different set of bands, so the block lives
// here rather than being spelled out twice. Mirrors OceanPushConstants in Scenes/OceanScene.cpp.
layout(push_constant) uniform OceanPushConstant {
    // Shared by every clipmap level, snapped on the cpu; see OceanClipmap in Crisp/Models/Ocean.hpp.
    layout(offset = 0) vec2 clipmapOrigin;
    layout(offset = 8) float clipmapFinestSpacing;
    layout(offset = 12) int clipmapBlockQuads;

    layout(offset = 16) float choppiness;
    layout(offset = 20) float waterRoughness;
    layout(offset = 24) float foamThreshold;
    layout(offset = 28) float foamSoftness;

    layout(offset = 32) float foamIntensity;
    layout(offset = 36) float invRmsWaveHeight;
    layout(offset = 40) float slopeVarianceScale;
    layout(offset = 44) float foamWindowSize;

    layout(offset = 48) int foamLayer;
    layout(offset = 52) float foamErosion;
    layout(offset = 56) float foamFreshness;
    // Sea level radius, in metres, of the same planet the atmosphere is built on.
    layout(offset = 60) float planetRadius;

    layout(offset = 64) vec4 cascadeSizes;
    // Geometric mean of each band's wavelength range, against which a sample spacing is judged.
    layout(offset = 80) vec4 cascadeWavelengths;
    // Per-axis slope variance of each band, folded into the specular lobe once the band is lost.
    layout(offset = 96) vec4 cascadeSlopeVariances;
};

#endif // CRISP_OCEAN_DRAW_GLSL
