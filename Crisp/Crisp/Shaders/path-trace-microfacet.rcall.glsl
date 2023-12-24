#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Parts/path-trace-payload.part.glsl"
#include "Parts/math-constants.part.glsl"
#include "Parts/warp.part.glsl"

layout(location = 0) callableDataInEXT BrdfSample brdf;

layout(set = 1, binding = 3) buffer BrdfParams {
    BrdfParameters brdfParams[];
};

vec3 ggxSampleNormal(in vec2 unitSample, in float alpha) {
    const float tanTheta2 = alpha * alpha * unitSample.y / (1.0f - unitSample.y);
    const float cosTheta = 1.0f / sqrt(1.0f + tanTheta2);
    const float phi = 2.0f * PI * unitSample.x;
    const float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

float ggxD(in vec3 normal, in float alpha) {
    const float cosTheta = normal.z;
    if (cosTheta <= 0.0f) {
        return 0.0f;
    }

    const float cosTheta2 = cosTheta * cosTheta;
    const float tanTheta2 = (1.0f - cosTheta2) / cosTheta2;
    const float alpha2 = alpha * alpha;
    const float sqTerm = alpha2 + tanTheta2;

    return alpha2 * InvPI / (cosTheta2 * cosTheta2 * sqTerm * sqTerm);
}

float tanTheta(in vec3 v) {
    const float sinTheta2 = 1.0f - v.z * v.z;
    if (sinTheta2 <= 0.0f) {
        return 0.0f;
    }

    return sqrt(sinTheta2) / v.z;
}

float ggxSmithBeckmann(in vec3 v, in vec3 microfacetNormal, in float alpha) {
    if (dot(v, microfacetNormal) * v.z <= 0.0f) {
        return 0.0f;
    }

    const float tanTheta = abs(tanTheta(v));
    if (tanTheta == 0.0f) {
        return 1.0f;
    }

    const float a = alpha * tanTheta;
    return 2.0f / (1.0f + sqrt(1.0f + a * a));
}

float ggxG(in vec3 wi, in vec3 wo, in vec3 m, in float alpha) {
    return ggxSmithBeckmann(wi, m, alpha) * ggxSmithBeckmann(wo, m, alpha);
}

float ggxPdf(in vec3 microfacetNormal, in float alpha)
{
    return ggxD(microfacetNormal, alpha) * abs(microfacetNormal.z);
}

float pdf(in vec3 wi, in vec3 wo, in float ks, in float alpha)
{
    const float diffusePdf = squareToCosineHemispherePdf(wo);

    const vec3 m = normalize(wi + wo); // Microfacet normal.
    const float Jh = 1.0f / (4.0f * dot(m, wo));
    const float microfacetPdf = ggxPdf(m, alpha) * Jh;

    return mix(diffusePdf, microfacetPdf, ks);
}

float fresnelDielectric(float cosThetaI, float extIOR, float intIOR, inout float cosThetaT)
{
    float etaI = extIOR, etaT = intIOR;

    // If indices of refraction are the same, no fresnel effects.
    if (extIOR == intIOR)
        return 0.0f;

    // if cosThetaI is < 0, it means the ray is coming from inside the object.
    if (cosThetaI < 0.0f)
    {
        float temp = etaI;
        etaI = etaT;
        etaT = temp;
        cosThetaI = -cosThetaI;
    }

    float eta = etaI / etaT;
    float sinThetaTSqr = eta * eta * (1.0f - cosThetaI * cosThetaI);

    // Total internal reflection.
    if (sinThetaTSqr > 1.0f)
        return 1.0f;

    cosThetaT = sqrt(1.0f - sinThetaTSqr);

    float Rs = (etaI * cosThetaI - etaT * cosThetaT) / (etaI * cosThetaI + etaT * cosThetaT);
    float Rp = (etaT * cosThetaI - etaI * cosThetaT) / (etaT * cosThetaI + etaI * cosThetaT);
    return (Rs * Rs + Rp * Rp) / 2.0f;
}

vec3 eval(in vec3 wi, in vec3 wo, in vec3 kd, in float ks, in float extIor, in float intIor, in float alpha)
{
    const float cosThetaI = wi.z;
    const float cosThetaO = wo.z;
    if (cosThetaI < 0.0f || cosThetaO < 0.0f) {
        return vec3(0.0f);
    }

    const vec3 microfacetNormal = normalize(wi + wo);

    const vec3 diffuse = kd * InvPI;

    float cosThetaT;
    const float F = fresnelDielectric(dot(wi, microfacetNormal), extIor, intIor, cosThetaT);
    const float D = ggxD(microfacetNormal, alpha);
    const float G = ggxG(wi, wo, microfacetNormal, alpha);

    const vec3 specular = vec3(ks * F * D * G);

    return diffuse * cosThetaO + specular / (4.0f * cosThetaI);
}

void main()
{
    const float ks = brdfParams[brdf.materialId].ks;
    const float alpha = brdfParams[brdf.materialId].microfacetAlpha;

    vec2 unitSample = brdf.unitSample;

    if (unitSample.x < ks) {
        unitSample.x /= ks; // Reuse the sample here.
        const vec3 microfacetNormal = ggxSampleNormal(unitSample, alpha);
        brdf.wo = 2.0f * dot(microfacetNormal, brdf.wi) * microfacetNormal - brdf.wi; // Reflect.
        brdf.lobeType = kLobeTypeGlossy;
    } else {
        unitSample.x = (unitSample.x - ks) / (1.0 - ks); // Reuse the sample here.
        brdf.wo = squareToCosineHemisphere(unitSample);
        brdf.lobeType = kLobeTypeDiffuse;
    }

    // Handle edge case if we are pointing into the surface due to bad microfacet sample reflection.
    if (brdf.wo.z < 0.0f) {
        brdf.f = vec3(0.0f);
        brdf.pdf = 0.0f;
        return;
    }

    brdf.pdf = pdf(brdf.wi, brdf.wo, ks, alpha);
    brdf.f = eval(
        brdf.wi,
        brdf.wo,
        brdfParams[brdf.materialId].kd,
        ks,
        brdfParams[brdf.materialId].extIor,
        brdfParams[brdf.materialId].intIor,
        alpha) / brdf.pdf;
}
