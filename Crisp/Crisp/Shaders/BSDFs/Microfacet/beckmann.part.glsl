#ifndef CRISP_MICROFACET_BECKMANN_GLSL
#define CRISP_MICROFACET_BECKMANN_GLSL

#include "common.part.glsl"

vec3 sampleBeckmannNormal(vec2 unitSample, float alpha) {
    const float tanThetaSquared = -alpha * alpha * log(max(1.0f - unitSample.y, 1e-7f));
    const float cosTheta = inversesqrt(1.0f + tanThetaSquared);
    const float phi = 2.0f * PI * unitSample.x;
    const float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

float beckmannDistribution(vec3 normal, float alpha) {
    if (normal.z <= 0.0f) {
        return 0.0f;
    }

    const float cosThetaSquared = normal.z * normal.z;
    const float tanThetaSquared = max(0.0f, 1.0f - cosThetaSquared) / cosThetaSquared;
    const float alphaSquared = alpha * alpha;
    return exp(-tanThetaSquared / alphaSquared) /
        (PI * alphaSquared * cosThetaSquared * cosThetaSquared);
}

float beckmannSmithG1(vec3 v, vec3 microfacetNormal, float alpha) {
    if (dot(v, microfacetNormal) * v.z <= 0.0f) {
        return 0.0f;
    }

    const float absTanTheta = abs(microfacetTanTheta(v));
    if (absTanTheta == 0.0f) {
        return 1.0f;
    }

    const float a = 1.0f / (alpha * absTanTheta);
    if (a >= 1.6f) {
        return 1.0f;
    }
    const float aSquared = a * a;
    return (3.535f * a + 2.181f * aSquared) /
        (1.0f + 2.276f * a + 2.577f * aSquared);
}

float beckmannGeometry(vec3 wi, vec3 wo, vec3 microfacetNormal, float alpha) {
    return beckmannSmithG1(wi, microfacetNormal, alpha) * beckmannSmithG1(wo, microfacetNormal, alpha);
}

float computeBeckmannNormalPdf(vec3 microfacetNormal, float alpha) {
    return beckmannDistribution(microfacetNormal, alpha) * abs(microfacetNormal.z);
}

#endif // CRISP_MICROFACET_BECKMANN_GLSL
