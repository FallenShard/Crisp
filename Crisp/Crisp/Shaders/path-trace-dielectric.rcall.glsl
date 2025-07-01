#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Common/path-trace-payload.part.glsl"
#include "Common/math-constants.part.glsl"

layout(location = 0) callableDataInEXT BrdfSample brdf;

layout(set = 1, binding = 3) buffer BrdfParams
{
    BrdfParameters brdfParams[];
};

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

void main()
{
    const float intIOR = brdfParams[brdf.materialId].intIor;
    const float extIOR = brdfParams[brdf.materialId].extIor;
    const float etaRatio = intIOR / extIOR;
    const float cosThetaI = dot(brdf.normal, brdf.wi);
    const vec3 localNormal = cosThetaI < 0.0f ? -brdf.normal : brdf.normal;
    const float eta        = cosThetaI < 0.0f ? etaRatio : 1.0f / etaRatio;
    const float cosine     = cosThetaI < 0.0f ? etaRatio * cosThetaI : -cosThetaI;
    float cosThetaT = 0.0f;
    const float fresnel = fresnelDielectric(cosThetaI, extIOR, intIOR, cosThetaT);

    brdf.lobeType = kLobeTypeDelta;
    if (brdf.unitSample[0] <= fresnel)
    {
        brdf.wo = reflect(-brdf.wi, localNormal);
        brdf.pdf = fresnel;
        brdf.f = vec3(1.0f);
    }
    else
    {
        brdf.wo = refract(-brdf.wi, localNormal, eta);
        brdf.pdf = 1.0f - fresnel;
        brdf.f = vec3(eta * eta);
    }
}
