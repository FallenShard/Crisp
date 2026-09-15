#ifndef CRISP_BSDF_SAMPLE_GLSL
#define CRISP_BSDF_SAMPLE_GLSL

// First: it declares the heap arrays that CRISP_GGX_ALBEDO_LUT expands to, which OpenPbr/surface.part.glsl
// reaches for at include time.
#include "../Textures/material-texture.part.glsl"

#include "../../BSDFs/OpenPbr/surface.part.glsl"
#include "../../BSDFs/lambertian.part.glsl"
#include "../../BSDFs/microfacet.part.glsl"
#include "../../BSDFs/mirror.part.glsl"
#include "../../BSDFs/oren-nayar.part.glsl"
#include "../../BSDFs/rough-conductor.part.glsl"
#include "../../BSDFs/rough-dielectric.part.glsl"
#include "../../BSDFs/smooth-conductor.part.glsl"
#include "../../BSDFs/smooth-dielectric.part.glsl"

// The sampling counterpart to bsdf-eval.part.glsl: one switch over the same tag, calling the same functions in
// Shaders/BSDFs. Sampling used to dispatch through executeCallableEXT, which made the material tag double as an
// SBT index -- a mismatch between the two orderings rendered a silently wrong material rather than failing
// validation. Switching inline retires that coupling, and the tag is now only a tag.

void sampleLambertianBsdf(const PbrMaterialParameters material, inout BsdfSample bsdf) {
    bsdf.wo = sampleLambertian(bsdf.unitSample);
    bsdf.lobeType = kLobeTypeDiffuse;
    bsdf.f = evaluateLambertian(evaluateMaterialReflectance(material, bsdf.texCoord), bsdf.wi, bsdf.wo);
    bsdf.pdf = computeLambertianPdf(bsdf.wi, bsdf.wo);
}

void sampleDielectricBsdf(const PbrMaterialParameters material, inout BsdfSample bsdf) {
    bsdf.lobeType = kLobeTypeDelta;
    sampleSmoothDielectric(
        bsdf.unitSample, bsdf.wi, kVacuumIor, material.surface.specularIor, bsdf.wo, bsdf.f, bsdf.pdf);
}

void sampleMirrorBsdf(inout BsdfSample bsdf) {
    bsdf.lobeType = kLobeTypeDelta;
    sampleMirror(bsdf.wi, bsdf.wo, bsdf.f, bsdf.pdf);
}

void sampleMicrofacetBsdf(const PbrMaterialParameters material, inout BsdfSample bsdf) {
    bool sampledSpecular;
    bsdf.wo = sampleMicrofacet(
        bsdf.unitSample,
        bsdf.lobeSample,
        bsdf.wi,
        material.surface.specularWeight,
        material.microfacetType,
        material.microfacetAlpha,
        sampledSpecular);
    bsdf.lobeType = sampledSpecular ? kLobeTypeGlossy : kLobeTypeDiffuse;

    bsdf.f = evaluateMicrofacet(
        material.surface.baseColor,
        material.surface.specularWeight,
        kVacuumIor,
        material.surface.specularIor,
        material.microfacetType,
        material.microfacetAlpha,
        bsdf.wi,
        bsdf.wo);
    bsdf.pdf = computeMicrofacetPdf(
        bsdf.wi, bsdf.wo, material.surface.specularWeight, material.microfacetType, material.microfacetAlpha);
}

void sampleOrenNayarBsdf(const PbrMaterialParameters material, inout BsdfSample bsdf) {
    bsdf.wo = sampleLambertian(bsdf.unitSample);
    bsdf.lobeType = kLobeTypeDiffuse;
    bsdf.f = evaluateOrenNayar(
        evaluateMaterialReflectance(material, bsdf.texCoord), material.orenNayarRoughness, bsdf.wi, bsdf.wo);
    bsdf.pdf = computeLambertianPdf(bsdf.wi, bsdf.wo);
}

void sampleSmoothConductorBsdf(const PbrMaterialParameters material, inout BsdfSample bsdf) {
    bsdf.lobeType = kLobeTypeDelta;
    sampleSmoothConductor(bsdf.wi, material.complexIorEta, material.complexIorK, bsdf.wo, bsdf.f, bsdf.pdf);
}

void sampleRoughConductorBsdf(const PbrMaterialParameters material, inout BsdfSample bsdf) {
    bsdf.wo = sampleRoughConductor(bsdf.unitSample, bsdf.wi, material.microfacetType, material.microfacetAlpha);
    bsdf.lobeType = kLobeTypeGlossy;
    bsdf.f = evaluateRoughConductor(
        material.complexIorEta,
        material.complexIorK,
        material.microfacetType,
        material.microfacetAlpha,
        bsdf.wi,
        bsdf.wo);
    bsdf.pdf = computeRoughConductorPdf(bsdf.wi, bsdf.wo, material.microfacetType, material.microfacetAlpha);
}

void sampleRoughDielectricBsdf(const PbrMaterialParameters material, inout BsdfSample bsdf) {
    bsdf.lobeType = kLobeTypeGlossy;
    sampleRoughDielectric(
        bsdf.unitSample,
        bsdf.lobeSample,
        kVacuumIor,
        material.surface.specularIor,
        material.microfacetType,
        material.microfacetAlpha,
        bsdf.wi,
        bsdf.wo,
        bsdf.f,
        bsdf.pdf);
}

// Reports the FULL mixture value and density, never just the lobe that was picked: the raygen weights
// next-event estimation against exactly this pdf. Compensation is a per-view setting the material record does
// not carry, so evaluateOpenPbr in bsdf-eval.part.glsl must agree with the mode chosen here.
void sampleOpenPbrBsdf(const PbrMaterialParameters material, inout BsdfSample bsdf) {
    const OpenPbrSurface surface = createOpenPbrSurface(material.surface, scene.energyCompensation);

    bool sampledSpecular = false;
    bsdf.weight =
        sampleOpenPbrSurface(surface, bsdf.unitSample, bsdf.lobeSample, bsdf.wi, bsdf.wo, bsdf.pdf, sampledSpecular);
    bsdf.f = bsdf.weight * bsdf.pdf; // Recover f (BSDF * abs(cosThetaO)) for callers that want the value itself.
    bsdf.lobeType = sampledSpecular ? kLobeTypeGlossy : kLobeTypeDiffuse;
}

void sampleBsdf(const PbrMaterialParameters material, inout BsdfSample bsdf) {
    switch (material.type) {
    case kBsdfLambertian:
        sampleLambertianBsdf(material, bsdf);
        break;
    case kBsdfDielectric:
        sampleDielectricBsdf(material, bsdf);
        break;
    case kBsdfMirror:
        sampleMirrorBsdf(bsdf);
        break;
    case kBsdfMicrofacet:
        sampleMicrofacetBsdf(material, bsdf);
        break;
    case kBsdfOrenNayar:
        sampleOrenNayarBsdf(material, bsdf);
        break;
    case kBsdfSmoothConductor:
        sampleSmoothConductorBsdf(material, bsdf);
        break;
    case kBsdfRoughConductor:
        sampleRoughConductorBsdf(material, bsdf);
        break;
    case kBsdfRoughDielectric:
        sampleRoughDielectricBsdf(material, bsdf);
        break;
    case kBsdfOpenPbr:
        sampleOpenPbrBsdf(material, bsdf);
        return;
    default: // An unknown tag absorbs the path rather than indexing past the table.
        bsdf.wo = vec3(0.0f);
        bsdf.f = vec3(0.0f);
        bsdf.pdf = 0.0f;
        bsdf.lobeType = kLobeTypeDiffuse;
        break;
    }

    bsdf.weight = bsdf.pdf > 0.0f ? bsdf.f / bsdf.pdf : vec3(0.0f);
}

#endif // CRISP_BSDF_SAMPLE_GLSL
