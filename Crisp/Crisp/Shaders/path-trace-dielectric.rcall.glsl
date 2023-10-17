#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Parts/path-tracer-payload.part.glsl"
#include "Parts/math-constants.part.glsl"

layout(location = 0) callableDataInEXT BsdfSample bsdfSample;

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
    const float intIOR = 1.5046f;
    const float extIOR = 1.00029f;
    const float etaRatio = intIOR / extIOR;
    const float cosThetaI = dot(bsdfSample.normal, bsdfSample.wi);
    const vec3 localNormal = cosThetaI < 0.0f ? -bsdfSample.normal : bsdfSample.normal;
    const float eta        = cosThetaI < 0.0f ? etaRatio : 1.0f / etaRatio;
    const float cosine     = cosThetaI < 0.0f ? etaRatio * cosThetaI : -cosThetaI;
    float cosThetaT = 0.0f;
    const float fresnel = fresnelDielectric(cosThetaI, extIOR, intIOR, cosThetaT);

    if (bsdfSample.unitSample[0] <= fresnel)
    {
        bsdfSample.sampleDirection = reflect(-bsdfSample.wi, localNormal);
        bsdfSample.samplePdf = fresnel;
        bsdfSample.eval = vec3(1.0f);
    }
    else
    {
        bsdfSample.sampleDirection = refract(-bsdfSample.wi, localNormal, eta);
        bsdfSample.samplePdf = 1.0f - fresnel;
        bsdfSample.eval = vec3(eta * eta);
    }
}
