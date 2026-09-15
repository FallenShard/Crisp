#ifndef CRISP_ROUGH_DIELECTRIC_GLSL
#define CRISP_ROUGH_DIELECTRIC_GLSL

#include "fresnel.part.glsl"
#include "microfacet.part.glsl"

bool roughDielectricHalfVector(
    vec3 wi, vec3 wo, float extIor, float intIor, out vec3 microfacetNormal, out float etaTi) {
    const bool reflection = wi.z * wo.z > 0.0f;
    etaTi = wi.z > 0.0f ? intIor / extIor : extIor / intIor;
    const vec3 halfVector = wi + wo * (reflection ? 1.0f : etaTi);
    const float lengthSquared = dot(halfVector, halfVector);
    if (wi.z == 0.0f || wo.z == 0.0f || lengthSquared <= 1e-14f) {
        microfacetNormal = vec3(0.0f, 0.0f, 1.0f);
        return false;
    }

    microfacetNormal = halfVector * inversesqrt(lengthSquared);
    if (microfacetNormal.z < 0.0f) {
        microfacetNormal = -microfacetNormal;
    }

    return dot(wi, microfacetNormal) * wi.z > 0.0f && dot(wo, microfacetNormal) * wo.z > 0.0f;
}

vec3 evaluateRoughDielectric(
    float extIor, float intIor, int microfacetType, float alpha, vec3 wi, vec3 wo) {
    if (abs(intIor - extIor) <= 1e-4f) {
        return vec3(0.0f);
    }

    vec3 microfacetNormal;
    float etaTi;
    if (!roughDielectricHalfVector(wi, wo, extIor, intIor, microfacetNormal, etaTi)) {
        return vec3(0.0f);
    }

    const bool reflection = wi.z * wo.z > 0.0f;
    const float cosThetaIm = dot(wi, microfacetNormal);
    const float cosThetaOm = dot(wo, microfacetNormal);
    float cosThetaTm;
    const float fresnel = fresnelDielectric(cosThetaIm, extIor, intIor, cosThetaTm);
    const float distribution = microfacetDistribution(microfacetNormal, microfacetType, alpha);
    const float geometry = microfacetGeometry(wi, wo, microfacetNormal, microfacetType, alpha);

    if (reflection) {
        return vec3(fresnel * distribution * geometry / (4.0f * abs(wi.z)));
    }

    const float denominator = cosThetaIm + etaTi * cosThetaOm;
    if (abs(denominator) <= 1e-7f) {
        return vec3(0.0f);
    }

    const float etaIt = 1.0f / etaTi;
    const float radianceScale = etaIt * etaIt;
    const float value = abs(
        radianceScale * (1.0f - fresnel) * distribution * geometry * etaTi * etaTi *
        cosThetaIm * cosThetaOm / (wi.z * denominator * denominator));
    return vec3(value);
}

float computeRoughDielectricPdf(
    float extIor, float intIor, int microfacetType, float alpha, vec3 wi, vec3 wo) {
    if (abs(intIor - extIor) <= 1e-4f) {
        return 0.0f;
    }

    vec3 microfacetNormal;
    float etaTi;
    if (!roughDielectricHalfVector(wi, wo, extIor, intIor, microfacetNormal, etaTi)) {
        return 0.0f;
    }

    const bool reflection = wi.z * wo.z > 0.0f;
    const float cosThetaIm = dot(wi, microfacetNormal);
    const float cosThetaOm = dot(wo, microfacetNormal);
    float cosThetaTm;
    const float fresnel = fresnelDielectric(cosThetaIm, extIor, intIor, cosThetaTm);
    const float normalPdf = computeMicrofacetVisibleNormalPdf(wi, microfacetNormal, microfacetType, alpha);

    if (reflection) {
        return fresnel * normalPdf / (4.0f * abs(cosThetaOm));
    }

    const float denominator = cosThetaIm + etaTi * cosThetaOm;
    if (abs(denominator) <= 1e-7f) {
        return 0.0f;
    }
    const float halfVectorJacobian = abs(etaTi * etaTi * cosThetaOm / (denominator * denominator));
    return (1.0f - fresnel) * normalPdf * halfVectorJacobian;
}

void sampleRoughDielectric(
    vec2 normalSample,
    float lobeSample,
    float extIor,
    float intIor,
    int microfacetType,
    float alpha,
    vec3 wi,
    out vec3 wo,
    out vec3 f,
    out float pdf) {
    wo = vec3(0.0f);
    f = vec3(0.0f);
    pdf = 0.0f;
    if (wi.z == 0.0f) {
        return;
    }

    if (abs(intIor - extIor) <= 1e-4f) {
        wo = -wi;
        f = vec3(1.0f);
        pdf = 1.0f;
        return;
    }

    const vec3 microfacetNormal = sampleMicrofacetVisibleNormal(normalSample, wi, microfacetType, alpha);
    const float cosThetaIm = dot(wi, microfacetNormal);
    if (cosThetaIm * wi.z <= 0.0f) {
        return;
    }

    float cosThetaTm;
    const float fresnel = fresnelDielectric(cosThetaIm, extIor, intIor, cosThetaTm);
    if (lobeSample <= fresnel) {
        wo = 2.0f * cosThetaIm * microfacetNormal - wi;
        if (wo.z * wi.z <= 0.0f) {
            wo = vec3(0.0f);
            return;
        }
    } else {
        const float etaIt = wi.z > 0.0f ? extIor / intIor : intIor / extIor;
        wo = microfacetNormal * (etaIt * cosThetaIm - sign(cosThetaIm) * cosThetaTm) - etaIt * wi;
        if (wo.z * wi.z >= 0.0f) {
            wo = vec3(0.0f);
            return;
        }
    }

    f = evaluateRoughDielectric(extIor, intIor, microfacetType, alpha, wi, wo);
    pdf = computeRoughDielectricPdf(extIor, intIor, microfacetType, alpha, wi, wo);
    if (pdf <= 0.0f) {
        wo = vec3(0.0f);
        f = vec3(0.0f);
        pdf = 0.0f;
    }
}

#endif // CRISP_ROUGH_DIELECTRIC_GLSL
