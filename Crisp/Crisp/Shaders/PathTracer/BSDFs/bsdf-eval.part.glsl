#ifndef CRISP_BSDF_EVAL_GLSL
#define CRISP_BSDF_EVAL_GLSL

#include "../../BSDFs/lambertian.part.glsl"
#include "../../BSDFs/microfacet.part.glsl"
#include "../../BSDFs/oren-nayar.part.glsl"
#include "../../BSDFs/rough-conductor.part.glsl"
#include "../../BSDFs/rough-dielectric.part.glsl"
#include "../../BSDFs/OpenPbr/surface.part.glsl"
#include "../Textures/material-texture.part.glsl"

BsdfEval evaluateLambertian(BsdfParameters material, vec2 texCoord, vec3 wi, vec3 wo) {
    return BsdfEval(
        evaluateLambertian(evaluateMaterialReflectance(material, texCoord), wi, wo), computeLambertianPdf(wi, wo));
}

BsdfEval evaluateMicrofacet(BsdfParameters material, vec3 wi, vec3 wo) {
    return BsdfEval(
        evaluateMicrofacet(
            material.surface.baseColor,
            material.surface.specularWeight,
            kVacuumIor,
            material.surface.specularIor,
            material.microfacetType,
            material.microfacetAlpha,
            wi,
            wo),
        computeMicrofacetPdf(
            wi, wo, material.surface.specularWeight, material.microfacetType, material.microfacetAlpha));
}

BsdfEval evaluateOrenNayar(BsdfParameters material, vec2 texCoord, vec3 wi, vec3 wo) {
    return BsdfEval(
        evaluateOrenNayar(
            evaluateMaterialReflectance(material, texCoord), material.orenNayarRoughness, wi, wo),
        computeLambertianPdf(wi, wo));
}

BsdfEval evaluateRoughConductor(BsdfParameters material, vec3 wi, vec3 wo) {
    return BsdfEval(
        evaluateRoughConductor(
            material.complexIorEta,
            material.complexIorK,
            material.microfacetType,
            material.microfacetAlpha,
            wi,
            wo),
        computeRoughConductorPdf(wi, wo, material.microfacetType, material.microfacetAlpha));
}

BsdfEval evaluateRoughDielectric(BsdfParameters material, vec3 wi, vec3 wo) {
    return BsdfEval(
        evaluateRoughDielectric(
            kVacuumIor,
            material.surface.specularIor,
            material.microfacetType,
            material.microfacetAlpha,
            wi,
            wo),
        computeRoughDielectricPdf(
            kVacuumIor,
            material.surface.specularIor,
            material.microfacetType,
            material.microfacetAlpha,
            wi,
            wo));
}

BsdfEval evaluateOpenPbr(BsdfParameters material, vec3 wi, vec3 wo) {
    // Compensation is a per-view setting the callable does not see, so next-event estimation and sampling agree
    // on None until it is plumbed through the material record. See docs/openpbr-path-tracer.md.
    const OpenPbrSurface surface = createOpenPbrSurface(material.surface, kEnergyCompensationNone);
    return BsdfEval(evaluateOpenPbrSurface(surface, wi, wo), computeOpenPbrSurfacePdf(surface, wi, wo));
}

BsdfEval evaluateBsdf(BsdfParameters material, vec2 texCoord, vec3 wi, vec3 wo) {
    switch (material.type) {
    case kBsdfLambertian:
        return evaluateLambertian(material, texCoord, wi, wo);
    case kBsdfMicrofacet:
        return evaluateMicrofacet(material, wi, wo);
    case kBsdfOrenNayar:
        return evaluateOrenNayar(material, texCoord, wi, wo);
    case kBsdfRoughConductor:
        return evaluateRoughConductor(material, wi, wo);
    case kBsdfRoughDielectric:
        return evaluateRoughDielectric(material, wi, wo);
    case kBsdfOpenPbr:
        return evaluateOpenPbr(material, wi, wo);
    default:
        return BsdfEval(vec3(0.0f), 0.0f); // Delta materials.
    }
}

#endif // CRISP_BSDF_EVAL_GLSL
