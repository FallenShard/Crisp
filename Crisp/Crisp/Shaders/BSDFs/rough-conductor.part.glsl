#ifndef CRISP_ROUGH_CONDUCTOR_GLSL
#define CRISP_ROUGH_CONDUCTOR_GLSL

#include "fresnel.part.glsl"
#include "microfacet.part.glsl"

vec3 evaluateRoughConductor(
    vec3 eta, vec3 k, int microfacetType, float alpha, vec3 wi, vec3 wo) {
    const float cosThetaI = wi.z;
    const float cosThetaO = wo.z;
    if (cosThetaI <= 0.0f || cosThetaO <= 0.0f) {
        return vec3(0.0f);
    }

    const vec3 microfacetNormal = microfacetReflectionHalfVector(wi, wo);
    const vec3 fresnel = fresnelConductor(dot(wi, microfacetNormal), eta, k);
    const float distribution = microfacetDistribution(microfacetNormal, microfacetType, alpha);
    const float geometry = microfacetGeometry(wi, wo, microfacetNormal, microfacetType, alpha);
    return fresnel * distribution * geometry / (4.0f * cosThetaI);
}

float computeRoughConductorPdf(vec3 wi, vec3 wo, int microfacetType, float alpha) {
    if (wi.z <= 0.0f || wo.z <= 0.0f) {
        return 0.0f;
    }

    const vec3 microfacetNormal = microfacetReflectionHalfVector(wi, wo);
    return computeMicrofacetNormalPdf(microfacetNormal, microfacetType, alpha) *
        microfacetReflectionJacobian(microfacetNormal, wo);
}

vec3 sampleRoughConductor(vec2 unitSample, vec3 wi, int microfacetType, float alpha) {
    const vec3 microfacetNormal = sampleMicrofacetNormal(unitSample, microfacetType, alpha);
    return 2.0f * dot(microfacetNormal, wi) * microfacetNormal - wi;
}

#endif // CRISP_ROUGH_CONDUCTOR_GLSL
