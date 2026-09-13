#ifndef CRISP_MICROFACET_COMMON_GLSL
#define CRISP_MICROFACET_COMMON_GLSL

#include "../../Common/math-constants.part.glsl"

const int kMicrofacetGgx = 0;
const int kMicrofacetBeckmann = 1;

vec3 fresnelSchlick(float cosTheta, vec3 f0) {
    return f0 + (1.0f - f0) * pow(1.0f - cosTheta, 5.0f);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 f0, float roughness) {
    return f0 + (max(vec3(1.0f - roughness), f0) - f0) * pow(1.0f - cosTheta, 5.0f);
}

float microfacetTanTheta(vec3 v) {
    const float sinThetaSquared = 1.0f - v.z * v.z;
    if (sinThetaSquared <= 0.0f) {
        return 0.0f;
    }

    return sqrt(sinThetaSquared) / v.z;
}

vec3 microfacetReflectionHalfVector(vec3 wi, vec3 wo) {
    return normalize(wi + wo);
}

float microfacetReflectionJacobian(vec3 microfacetNormal, vec3 wo) {
    return 1.0f / (4.0f * dot(microfacetNormal, wo));
}

#endif // CRISP_MICROFACET_COMMON_GLSL
