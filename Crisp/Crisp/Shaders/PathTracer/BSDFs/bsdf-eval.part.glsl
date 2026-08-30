#ifndef CRISP_BRDF_EVAL_GLSL
#define CRISP_BRDF_EVAL_GLSL

#include "../../Brdf/lambertian.part.glsl"
#include "../../Brdf/microfacet.part.glsl"
#include "../../Brdf/oren-nayar.part.glsl"
#include "../../Brdf/rough-conductor.part.glsl"
#include "../../Brdf/rough-dielectric.part.glsl"
#include "../Textures/material-texture.part.glsl"

BrdfEval evaluateLambertian(BrdfParameters material, vec2 texCoord, vec3 wi, vec3 wo) {
    return BrdfEval(
        evaluateLambertian(evaluateMaterialReflectance(material, texCoord), wi, wo), lambertianPdf(wi, wo));
}

BrdfEval evaluateMicrofacet(BrdfParameters material, vec3 wi, vec3 wo) {
    return BrdfEval(
        evaluateMicrofacet(
            material.kd,
            material.ks,
            material.extIor,
            material.intIor,
            material.microfacetType,
            material.microfacetAlpha,
            wi,
            wo),
        microfacetPdf(wi, wo, material.ks, material.microfacetType, material.microfacetAlpha));
}

BrdfEval evaluateOrenNayar(BrdfParameters material, vec2 texCoord, vec3 wi, vec3 wo) {
    return BrdfEval(
        evaluateOrenNayar(evaluateMaterialReflectance(material, texCoord), material.roughness, wi, wo),
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
            material.extIor,
            material.intIor,
            material.microfacetType,
            material.microfacetAlpha,
            wi,
            wo),
        roughDielectricPdf(
            material.extIor,
            material.intIor,
            material.microfacetType,
            material.microfacetAlpha,
            wi,
            wo));
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
    default:
        return BrdfEval(vec3(0.0f), 0.0f); // Delta materials.
    }
}

#endif // CRISP_BRDF_EVAL_GLSL
