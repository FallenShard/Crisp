#ifndef CRISP_SMOOTH_DIELECTRIC_GLSL
#define CRISP_SMOOTH_DIELECTRIC_GLSL

#include "fresnel.part.glsl"

void sampleSmoothDielectric(
    const vec2 unitSample,
    const vec3 wi,
    const float exteriorIor,
    const float interiorIor,
    out vec3 wo,
    out vec3 f,
    out float pdf) {
    const float etaRatio = interiorIor / exteriorIor;
    const float cosThetaI = wi.z;
    const vec3 localNormal = vec3(0.0f, 0.0f, cosThetaI < 0.0f ? -1.0f : 1.0f);
    const float eta = cosThetaI < 0.0f ? etaRatio : 1.0f / etaRatio;
    float cosThetaT = 0.0f;
    const float fresnel = fresnelDielectric(cosThetaI, exteriorIor, interiorIor, cosThetaT);

    if (unitSample.x <= fresnel) {
        wo = reflect(-wi, localNormal);
        pdf = fresnel;
        f = vec3(fresnel);
        return;
    }

    wo = refract(-wi, localNormal, eta);
    pdf = 1.0f - fresnel;
    f = vec3(pdf * eta * eta);
}

#endif // CRISP_SMOOTH_DIELECTRIC_GLSL
