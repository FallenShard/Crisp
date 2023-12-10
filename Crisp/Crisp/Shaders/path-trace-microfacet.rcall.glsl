#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Parts/path-tracer-payload.part.glsl"
#include "Parts/math-constants.part.glsl"
#include "Parts/warp.part.glsl"

layout(location = 0) callableDataInEXT BsdfSample bsdf;

layout(set = 1, binding = 3) buffer BrdfParams
{
    BrdfParameters brdfParams[];
};

vec3 sampleNormal(vec2 unitSample, float alpha)
{
    float tanTheta2 = alpha * alpha * unitSample.y / (1.0f - unitSample.y);
    float cosTheta = 1.0f / sqrt(1.0f + tanTheta2);
    float phi = 2.0f * PI * unitSample.x;
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

float ggxD(in vec3 normal, in float alpha) {
    float cosTheta = abs(normal.z);
    if (cosTheta <= 0.0f) {
        return 0.0f;
    }

    float cosTheta2 = cosTheta * cosTheta;
    float tanTheta2 = (1.0f - cosTheta2) / cosTheta2;
    float alpha2 = alpha * alpha;
    float sqTerm = alpha2 + tanTheta2;

    return alpha2 * InvPI / (cosTheta2 * cosTheta2 * sqTerm * sqTerm);
}

float tanTheta(in vec3 v) {
    float sinTheta2 = 1.0f - v.z * v.z;
    if (sinTheta2 <= 0.0f) {
        return 0.0f;
    }

    return sqrt(sinTheta2) / v.z;
}

float ggxSmithBeckmann(in vec3 v, in vec3 m, in float alpha) {
    if (dot(v, m) * abs(v.z) <= 0.0f) {
        return 0.0f;
    }

    float tanTheta = abs(tanTheta(v));
    if (tanTheta == 0.0f) {
        return 1.0f;
    }

    const float a = alpha * tanTheta;
    return 2.0f / (1.0f + sqrt(1.0f + a * a));
}

float ggxG(in vec3 wi, in vec3 wo, in vec3 m, in float alpha) {
    return ggxSmithBeckmann(wi, m, alpha) * ggxSmithBeckmann(wo, m, alpha);
}

float ggxPdf(in vec3 normal, in float alpha)
{
    return ggxD(normal, alpha) * abs(normal.z);
}

float pdf(in vec3 wi, in vec3 wo, in float ks, in float alpha)
{
    const vec3 m = normalize(wi + wo);

    const float Jh = 1.0f / (4.0f * dot(m, wo));
    const float specPdf = ks * ggxPdf(m, alpha) * Jh;
    const float diffPdf = (1.0f - ks) * squareToCosineHemispherePdf(wo);

    return specPdf + diffPdf;
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
    const float cosThetaI = abs(wi.z);
    const float cosThetaO = abs(wo.z);

    const vec3 m = normalize(wi + wo);

    const vec3 diffuse = kd * InvPI;

    float cosThetaT;
    float F = fresnelDielectric(dot(wi, m), extIor, intIor, cosThetaT);
    float D = ggxD(m, alpha);
    float G = ggxG(wi, wo, m, alpha);

    const vec3 specular = vec3(ks * F * D * G);

    return diffuse * cosThetaO + specular / (4.0f * cosThetaI);
}

void main()
{
    const float ks = brdfParams[bsdf.materialId].ks;
    const float alpha = brdfParams[bsdf.materialId].microfacetAlpha * brdfParams[bsdf.materialId].microfacetAlpha;

    vec2 unitSample = bsdf.unitSample;

    if (unitSample.x < ks) {
        unitSample.x /= ks;
        const vec3 dir = sampleNormal(unitSample, alpha);
        bsdf.wo = 2.0f * dot(dir, bsdf.wi) * dir - bsdf.wi;
    } else {
        unitSample.x = (unitSample.x - ks) / (1.0 - ks);
        bsdf.wo = squareToCosineHemisphere(unitSample);
    }

    if (bsdf.wo.z < 0.0f) {
        bsdf.f = vec3(0.0f);
        bsdf.pdf = 0.0f;
        return;
    }

    bsdf.pdf = pdf(bsdf.wi, bsdf.wo, ks, alpha);
    bsdf.f = eval(
        bsdf.wi,
        bsdf.wo,
        brdfParams[bsdf.materialId].kd,
        ks,
        brdfParams[bsdf.materialId].extIor,
        brdfParams[bsdf.materialId].intIor,
        alpha) / bsdf.pdf;
}
