#ifndef CRISP_OPENPBR_SURFACE_GLSL
#define CRISP_OPENPBR_SURFACE_GLSL

#include "../../Common/math-constants.part.glsl"
#include "../../Common/openpbr-surface.part.glsl"
#include "../../Common/warp.part.glsl"
#include "../lambertian.part.glsl"
#include "../oren-nayar.part.glsl"
#include "../Microfacet/ggx.part.glsl"
#include "energy-compensation.part.glsl"
#include "layering.part.glsl"

// The OpenPBR opaque surface, evaluated and sampled from one place. The sample and evaluate switches in
// PathTracer/BSDFs both enter through these functions rather than reimplementing them, or sampling and
// evaluation drift apart and MIS is silently biased.
//
// The diffuse lobe is the specified one -- Oren-Nayar, in its energy-preserving form; see
// BSDFs/oren-nayar.part.glsl. The specular half is still the split-sum-shaped approximation the rasterizer
// uses rather than OpenPBR proper: a Schlick Fresnel, a metalness lerp on F0, and a (1 - F) diffuse factor.
// Reaching the actual model from here means albedo-scaled layering via layering.part.glsl and an F82-tint
// metal lobe.

struct OpenPbrSurface {
    vec3 diffuseAlbedo;
    vec3 f0;
    float alpha;
    float diffuseRoughness;
    uint energyCompensation;
};

float openPbrLuminance(const vec3 value) {
    return dot(value, vec3(0.2126f, 0.7152f, 0.0722f));
}

OpenPbrSurface createOpenPbrSurface(const OpenPbrSurfaceParams params, const uint energyCompensation) {
    const float metalness = clamp(params.baseMetalness, 0.0f, 1.0f);
    const vec3 baseColor = max(params.baseColor, vec3(0.0f)) * max(params.baseWeight, 0.0f);
    const vec3 dielectric =
        vec3(openPbrDielectricF0(params.specularIor, params.specularWeight)) *
        clamp(params.specularColor, 0.0f, 1.0f);

    OpenPbrSurface surface;
    surface.diffuseAlbedo = baseColor * (1.0f - metalness);
    surface.f0 = mix(dielectric, baseColor, metalness);
    surface.alpha = params.specularRoughness * params.specularRoughness;
    surface.diffuseRoughness = params.baseDiffuseRoughness;
    surface.energyCompensation = energyCompensation;
    return surface;
}

float openPbrSpecularProbability(const OpenPbrSurface surface, const float cosThetaI) {
    const vec3 layerAlbedo = specularDirectionalAlbedo(cosThetaI, surface.alpha, surface.f0);
    const float specularWeight = openPbrLuminance(layerAlbedo);
    const float diffuseWeight = openPbrLuminance((1.0f - layerAlbedo) * surface.diffuseAlbedo);

    if (diffuseWeight <= 0.0f) {
        return 1.0f;
    }
    if (specularWeight <= 0.0f) {
        return 0.0f;
    }

    return clamp(specularWeight / (specularWeight + diffuseWeight), 0.05f, 0.95f);
}

vec3 evaluateOpenPbrSurface(const OpenPbrSurface surface, const vec3 wi, const vec3 wo) {
    if (wi.z <= 0.0f || wo.z <= 0.0f) {
        return vec3(0.0f);
    }

    const vec3 halfVector = microfacetReflectionHalfVector(wi, wo);
    const vec3 fresnel = fresnelSchlick(max(dot(wi, halfVector), 0.0f), surface.f0);

    const vec3 diffuse = evaluateOrenNayarBsdf(surface.diffuseAlbedo, surface.diffuseRoughness, wi.z, wo.z, dot(wi, wo));
    const float distribution = ggxDistribution(halfVector, surface.alpha);
    const float geometry = ggxGeometry(wi, wo, halfVector, surface.alpha);
    vec3 specular = fresnel * distribution * geometry / (4.0f * wi.z * wo.z);

    if (surface.energyCompensation == kEnergyCompensationTurquin) {
        specular *= turquinScale(wi.z, surface.alpha, schlickFresnelAverage(surface.f0));
    } else if (surface.energyCompensation == kEnergyCompensationKullaConty) {
        specular += kullaContyLobe(wi.z, wo.z, surface.alpha, schlickFresnelAverage(surface.f0));
    }

    const vec3 layerAlbedo = specularDirectionalAlbedo(wi.z, surface.alpha, surface.f0);
    return composeLayer(specular, diffuse, layerAlbedo, kUnitTransmittance) * wo.z;
}

// The full mixture density, not the sampled lobe's. Next-event estimation and BSDF sampling both weight with
// this, so returning only the sampled lobe would bias every MIS combination.
float computeOpenPbrSurfacePdf(const OpenPbrSurface surface, const vec3 wi, const vec3 wo) {
    if (wi.z <= 0.0f || wo.z <= 0.0f) {
        return 0.0f;
    }

    const vec3 halfVector = microfacetReflectionHalfVector(wi, wo);
    const float specularPdf =
        computeGgxVisibleNormalPdf(wi, halfVector, surface.alpha) /
        (4.0f * max(dot(halfVector, wo), 1e-6f));
    const float diffusePdf = wo.z / PI;
    return mix(diffusePdf, specularPdf, openPbrSpecularProbability(surface, wi.z));
}

// Returns f * cos(wo) / pdf for the sampled direction, or zero when the sample leaves the upper hemisphere.
vec3 sampleOpenPbrSurface(
    const OpenPbrSurface surface,
    const vec2 unitSample,
    const float lobeSample,
    const vec3 wi,
    out vec3 wo,
    out float pdf,
    out bool sampledSpecular) {
    sampledSpecular = lobeSample < openPbrSpecularProbability(surface, wi.z);
    if (sampledSpecular) {
        const vec3 microfacetNormal = sampleGgxVisibleNormal(unitSample, wi, surface.alpha);
        wo = 2.0f * dot(microfacetNormal, wi) * microfacetNormal - wi;
    } else {
        wo = squareToCosineHemisphere(unitSample);
    }

    pdf = computeOpenPbrSurfacePdf(surface, wi, wo);
    if (pdf <= 0.0f || wo.z <= 0.0f) {
        pdf = 0.0f;
        return vec3(0.0f);
    }

    return evaluateOpenPbrSurface(surface, wi, wo) / pdf;
}

#endif // CRISP_OPENPBR_SURFACE_GLSL
