#ifndef CRISP_MICROFACET_GLSL
#define CRISP_MICROFACET_GLSL

#include "fresnel.part.glsl"
#include "../Common/math-constants.part.glsl"
#include "../Common/warp.part.glsl"
#include "Microfacet/common.part.glsl"
#include "Microfacet/ggx.part.glsl"
#include "Microfacet/beckmann.part.glsl"

vec3 sampleMicrofacetNormal(vec2 unitSample, int microfacetType, float alpha) {
    return microfacetType == kMicrofacetBeckmann
        ? sampleBeckmannNormal(unitSample, alpha)
        : sampleGgxNormal(unitSample, alpha);
}

vec3 sampleMicrofacetVisibleNormal(
    const vec2 unitSample, const vec3 wi, const int microfacetType, const float alpha) {
    return microfacetType == kMicrofacetBeckmann
        ? sampleBeckmannVisibleNormal(unitSample, wi, alpha)
        : sampleGgxVisibleNormal(unitSample, wi, alpha);
}

vec3 sampleMicrofacet(
    const vec2 unitSample,
    const float lobeSample,
    const vec3 wi,
    const float specularProbability,
    const int microfacetType,
    const float alpha,
    out bool sampledSpecular) {
    sampledSpecular = lobeSample < specularProbability;
    if (sampledSpecular) {
        const vec3 microfacetNormal = sampleMicrofacetVisibleNormal(unitSample, wi, microfacetType, alpha);
        return 2.0f * dot(microfacetNormal, wi) * microfacetNormal - wi;
    }

    return squareToCosineHemisphere(unitSample);
}

float microfacetDistribution(vec3 normal, int microfacetType, float alpha) {
    return microfacetType == kMicrofacetBeckmann
        ? beckmannDistribution(normal, alpha)
        : ggxDistribution(normal, alpha);
}

float microfacetGeometry(vec3 wi, vec3 wo, vec3 microfacetNormal, int microfacetType, float alpha) {
    return microfacetType == kMicrofacetBeckmann
        ? beckmannGeometry(wi, wo, microfacetNormal, alpha)
        : ggxGeometry(wi, wo, microfacetNormal, alpha);
}

float computeMicrofacetNormalPdf(vec3 microfacetNormal, int microfacetType, float alpha) {
    return microfacetType == kMicrofacetBeckmann
        ? computeBeckmannNormalPdf(microfacetNormal, alpha)
        : computeGgxNormalPdf(microfacetNormal, alpha);
}

float computeMicrofacetVisibleNormalPdf(
    const vec3 wi, const vec3 microfacetNormal, const int microfacetType, const float alpha) {
    return microfacetType == kMicrofacetBeckmann
        ? computeBeckmannVisibleNormalPdf(wi, microfacetNormal, alpha)
        : computeGgxVisibleNormalPdf(wi, microfacetNormal, alpha);
}

float computeMicrofacetPdf(vec3 wi, vec3 wo, float ks, int microfacetType, float alpha) {
    if (wi.z <= 0.0f || wo.z <= 0.0f) {
        return 0.0f;
    }

    const float diffusePdf = wo.z / PI;
    const vec3 microfacetNormal = microfacetReflectionHalfVector(wi, wo);
    const float specularPdf = computeMicrofacetVisibleNormalPdf(wi, microfacetNormal, microfacetType, alpha) *
        microfacetReflectionJacobian(microfacetNormal, wo);

    return mix(diffusePdf, specularPdf, ks);
}

vec3 evaluateMicrofacet(
    vec3 kd, float ks, float extIor, float intIor, int microfacetType, float alpha, vec3 wi, vec3 wo) {
    const float cosThetaI = wi.z;
    const float cosThetaO = wo.z;
    if (cosThetaI <= 0.0f || cosThetaO <= 0.0f) {
        return vec3(0.0f);
    }

    const vec3 microfacetNormal = microfacetReflectionHalfVector(wi, wo);
    const vec3 diffuse = kd * InvPI;

    float cosThetaT;
    const float fresnel = fresnelDielectric(dot(wi, microfacetNormal), extIor, intIor, cosThetaT);
    const float distribution = microfacetDistribution(microfacetNormal, microfacetType, alpha);
    const float geometry = microfacetGeometry(wi, wo, microfacetNormal, microfacetType, alpha);
    const vec3 specular = vec3(ks * fresnel * distribution * geometry);

    return diffuse * cosThetaO + specular / (4.0f * cosThetaI);
}

#endif // CRISP_MICROFACET_GLSL
