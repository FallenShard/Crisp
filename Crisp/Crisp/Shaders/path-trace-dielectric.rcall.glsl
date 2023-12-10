#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Parts/path-tracer-payload.part.glsl"
#include "Parts/math-constants.part.glsl"

layout(location = 0) callableDataInEXT BsdfSample bsdf;

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
    const float intIOR = brdfParams[bsdf.materialId].intIor;
    const float extIOR = brdfParams[bsdf.materialId].extIor;
    const float etaRatio = intIOR / extIOR;
    const float cosThetaI = dot(bsdf.normal, bsdf.wi);
    const vec3 localNormal = cosThetaI < 0.0f ? -bsdf.normal : bsdf.normal;
    const float eta        = cosThetaI < 0.0f ? etaRatio : 1.0f / etaRatio;
    const float cosine     = cosThetaI < 0.0f ? etaRatio * cosThetaI : -cosThetaI;
    float cosThetaT = 0.0f;
    const float fresnel = fresnelDielectric(cosThetaI, extIOR, intIOR, cosThetaT);

    if (bsdf.unitSample[0] <= fresnel)
    {
        bsdf.wo = reflect(-bsdf.wi, localNormal);
        bsdf.pdf = fresnel;
        bsdf.f = vec3(1.0f);
    }
    else
    {
        bsdf.wo = refract(-bsdf.wi, localNormal, eta);
        bsdf.pdf = 1.0f - fresnel;
        bsdf.f = vec3(eta * eta);
    }
}
