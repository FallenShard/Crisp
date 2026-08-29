#ifndef CRISP_BRDF_EVAL_GLSL
#define CRISP_BRDF_EVAL_GLSL

#include "../../Brdf/lambertian.part.glsl"
#include "../../Brdf/microfacet.part.glsl"
#include "../../Brdf/oren-nayar.part.glsl"

BrdfEval evaluateLambertian(BrdfParameters material, vec3 wi, vec3 wo) {
    return BrdfEval(evaluateLambertian(material.albedo, wi, wo), lambertianPdf(wi, wo));
}

BrdfEval evaluateMicrofacet(BrdfParameters material, vec3 wi, vec3 wo) {
    return BrdfEval(
        evaluateMicrofacet(material.kd, material.ks, material.extIor, material.intIor, material.microfacetAlpha, wi, wo),
        microfacetPdf(wi, wo, material.ks, material.microfacetAlpha));
}

BrdfEval evaluateOrenNayar(BrdfParameters material, vec3 wi, vec3 wo) {
    return BrdfEval(
        evaluateOrenNayar(material.albedo, material.roughness, wi, wo),
        lambertianPdf(wi, wo));
}

BrdfEval evaluateBrdf(BrdfParameters material, vec3 wi, vec3 wo) {
    switch (material.type) {
    case kBrdfLambertian:
        return evaluateLambertian(material, wi, wo);
    case kBrdfMicrofacet:
        return evaluateMicrofacet(material, wi, wo);
    case kBrdfOrenNayar:
        return evaluateOrenNayar(material, wi, wo);
    default:
        return BrdfEval(vec3(0.0f), 0.0f); // Delta materials.
    }
}

#endif // CRISP_BRDF_EVAL_GLSL
