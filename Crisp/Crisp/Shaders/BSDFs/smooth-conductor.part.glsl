#ifndef CRISP_SMOOTH_CONDUCTOR_GLSL
#define CRISP_SMOOTH_CONDUCTOR_GLSL

#include "fresnel.part.glsl"

void sampleSmoothConductor(
    const vec3 wi,
    const vec3 eta,
    const vec3 k,
    out vec3 wo,
    out vec3 weight,
    out float pdf) {
    const float cosThetaI = wi.z;
    if (cosThetaI <= 0.0f) {
        wo = vec3(0.0f);
        weight = vec3(0.0f);
        pdf = 0.0f;
        return;
    }

    wo = vec3(-wi.xy, wi.z);
    weight = fresnelConductor(cosThetaI, eta, k);
    pdf = 1.0f;
}

#endif // CRISP_SMOOTH_CONDUCTOR_GLSL
