#ifndef CRISP_OPENPBR_ENERGY_COMPENSATION_GLSL
#define CRISP_OPENPBR_ENERGY_COMPENSATION_GLSL

#include "../../Common/math-constants.part.glsl"
#include "directional-albedo.part.glsl"

// Multiple-scattering compensation for the single-scattering GGX lobe. OpenPBR names these and mandates none,
// so they sit side by side for the material explorer to switch between under the white furnace.
// Unbiased solution by Heitz, 2016, is at https://dl.acm.org/doi/10.1145/2897824.2925943.

const uint kEnergyCompensationNone = 0;
const uint kEnergyCompensationKullaConty = 1;
const uint kEnergyCompensationTurquin = 2;

// 2 * integral F(mu) mu dmu, which closes because integral (1 - mu)^5 mu dmu is the beta function B(2, 6) = 1/42.
vec3 schlickFresnelAverage(const vec3 f0) {
    return (20.0f * f0 + 1.0f) / 21.0f;
}

// Kulla-Conty 2017, https://blog.selfshadow.com/publications/s2017-shading-course/imageworks/s2017_pbs_imageworks_slides_v2.pdf
// Symmetric in wi/wo, so it keeps the reciprocity BsdfValidationTest asserts. Additive: f = f_single + this.
vec3 kullaContyLobe(const float cosThetaI, const float cosThetaO, const float alpha, const vec3 fresnelAverage) {
    const float albedoI = ggxDirectionalAlbedo(cosThetaI, alpha);
    const float albedoO = ggxDirectionalAlbedo(cosThetaO, alpha);
    const float albedoAverage = ggxAverageAlbedo(alpha);
    if (albedoAverage >= 1.0f) {
        return vec3(0.0f);
    }

    const float white = (1.0f - albedoI) * (1.0f - albedoO) / (PI * (1.0f - albedoAverage));

    // A coloured conductor saturates with each bounce, so compensated rough metal gets more saturated, not
    // merely brighter.
    const vec3 tint = fresnelAverage * fresnelAverage * albedoAverage /
        max(vec3(1e-4f), 1.0f - fresnelAverage * (1.0f - albedoAverage));

    return white * tint;
}

// Turquin 2019, https://blog.selfshadow.com/publications/turquin/ms_comp_final.pdf
// Cheaper than Kulla-Conty but trades away reciprocity. Multiplicative: f = f_single * this.
//
// The cosine must be the one held FIXED while the hemisphere is integrated -- the view direction, wi here.
// Passing the sampled direction puts the scale inside the integral and the furnace stops closing to 1.
vec3 turquinScale(const float cosThetaView, const float alpha, const vec3 fresnelAverage) {
    const float albedo = ggxDirectionalAlbedo(cosThetaView, alpha);
    if (albedo <= 0.0f) {
        return vec3(1.0f);
    }

    return 1.0f + fresnelAverage * (1.0f - albedo) / albedo;
}

#endif // CRISP_OPENPBR_ENERGY_COMPENSATION_GLSL
