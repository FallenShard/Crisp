#ifndef CRISP_BRDF_EVAL_GLSL
#define CRISP_BRDF_EVAL_GLSL

#include "../../BSDFs/lambertian.part.glsl"
#include "../../BSDFs/microfacet.part.glsl"
#include "../../BSDFs/oren-nayar.part.glsl"
#include "../../BSDFs/rough-conductor.part.glsl"
#include "../../BSDFs/rough-dielectric.part.glsl"
#include "../../BSDFs/OpenPbr/surface.part.glsl"
#include "../Textures/material-texture.part.glsl"

BrdfEval evaluateLambertian(BrdfParameters material, vec2 texCoord, vec3 wi, vec3 wo) {
    return BrdfEval(
        evaluateLambertian(evaluateMaterialReflectance(material, texCoord), wi, wo), lambertianPdf(wi, wo));
}

BrdfEval evaluateMicrofacet(BrdfParameters material, vec3 wi, vec3 wo) {
    return BrdfEval(
        evaluateMicrofacet(
            material.surface.baseColor,
            material.surface.specularWeight,
            material.exteriorIor,
            material.surface.specularIor,
            material.microfacetType,
            material.microfacetAlpha,
            wi,
            wo),
        microfacetPdf(wi, wo, material.surface.specularWeight, material.microfacetType, material.microfacetAlpha));
}

BrdfEval evaluateOrenNayar(BrdfParameters material, vec2 texCoord, vec3 wi, vec3 wo) {
    return BrdfEval(
        evaluateOrenNayar(
            evaluateMaterialReflectance(material, texCoord), material.orenNayarRoughness, wi, wo),
        lambertianPdf(wi, wo));
}

BrdfEval evaluateRoughConductor(BrdfParameters material, vec3 wi, vec3 wo) {
    return BrdfEval(
        evaluateRoughConductor(
            material.complexIorEta,
            material.complexIorK,
            material.microfacetType,
            material.microfacetAlpha,
            wi,
            wo),
        roughConductorPdf(wi, wo, material.microfacetType, material.microfacetAlpha));
}

BrdfEval evaluateRoughDielectric(BrdfParameters material, vec3 wi, vec3 wo) {
    return BrdfEval(
        evaluateRoughDielectric(
            material.exteriorIor,
            material.surface.specularIor,
            material.microfacetType,
            material.microfacetAlpha,
            wi,
            wo),
        roughDielectricPdf(
            material.exteriorIor,
            material.surface.specularIor,
            material.microfacetType,
            material.microfacetAlpha,
            wi,
            wo));
}

BrdfEval evaluateOpenPbr(BrdfParameters material, vec3 wi, vec3 wo) {
    // Compensation is a per-view setting the callable does not see, so next-event estimation and sampling agree
    // on None until it is plumbed through the material record. See docs/openpbr-path-tracer.md.
    const OpenPbrSurface surface = createOpenPbrSurface(material.surface, kEnergyCompensationNone);
    return BrdfEval(evaluateOpenPbrSurface(surface, wi, wo), openPbrSurfacePdf(surface, wi, wo));
}

BrdfEval evaluateBrdf(BrdfParameters material, vec2 texCoord, vec3 wi, vec3 wo) {
    switch (material.type) {
    case kBrdfLambertian:
        return evaluateLambertian(material, texCoord, wi, wo);
    case kBrdfMicrofacet:
        return evaluateMicrofacet(material, wi, wo);
    case kBrdfOrenNayar:
        return evaluateOrenNayar(material, texCoord, wi, wo);
    case kBrdfRoughConductor:
        return evaluateRoughConductor(material, wi, wo);
    case kBrdfRoughDielectric:
        return evaluateRoughDielectric(material, wi, wo);
    case kBrdfOpenPbr:
        return evaluateOpenPbr(material, wi, wo);
    default:
        return BrdfEval(vec3(0.0f), 0.0f); // Delta materials.
    }
}

#endif // CRISP_BRDF_EVAL_GLSL
