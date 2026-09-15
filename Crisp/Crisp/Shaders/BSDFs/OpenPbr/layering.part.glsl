#ifndef CRISP_OPENPBR_LAYERING_GLSL
#define CRISP_OPENPBR_LAYERING_GLSL

#include "directional-albedo.part.glsl"

// OpenPBR's albedo-scaling layer composition. The substrate receives exactly the energy the layer above did not
// reflect, so the composite conserves energy whenever both layers do:
//
//   f_layer = f_coat + T_coat * (1 - E_coat) * f_sub
//
// This is what replaces the ad-hoc couplings currently in the tree -- the hand-authored ks split in
// BSDFs/microfacet.part.glsl and the (1 - F) diffuse factor the path tracer used before it. Neither
// composes correctly.

// Transmittance of the layer above, for a non-absorbing layer. Coat absorption multiplies into this later.
const float kUnitTransmittance = 1.0f;

vec3 composeLayer(
    const vec3 coat, const vec3 substrate, const float coatDirectionalAlbedo, const float coatTransmittance) {
    return coat + coatTransmittance * (1.0f - coatDirectionalAlbedo) * substrate;
}

vec3 composeLayer(
    const vec3 coat, const vec3 substrate, const vec3 coatDirectionalAlbedo, const float coatTransmittance) {
    return coat + coatTransmittance * (1.0f - coatDirectionalAlbedo) * substrate;
}

// Weighted form, for a coat whose presence is itself a weight rather than all-or-nothing. Lerping the substrate
// factor rather than the substrate keeps weight zero exactly equal to the unlayered substrate.
vec3 composeWeightedLayer(
    const vec3 coat,
    const vec3 substrate,
    const float coatDirectionalAlbedo,
    const float coatTransmittance,
    const float coatWeight) {
    const float substrateFactor = mix(1.0f, coatTransmittance * (1.0f - coatDirectionalAlbedo), coatWeight);
    return coatWeight * coat + substrateFactor * substrate;
}

#endif // CRISP_OPENPBR_LAYERING_GLSL
