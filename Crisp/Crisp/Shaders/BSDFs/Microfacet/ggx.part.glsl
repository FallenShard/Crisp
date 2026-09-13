#ifndef CRISP_MICROFACET_GGX_GLSL
#define CRISP_MICROFACET_GGX_GLSL

#include "common.part.glsl"

float distributionGgx(float nDotH, float alpha) {
    const float a2 = max(alpha * alpha, 1e-12f);
    const float denom = a2 * nDotH * nDotH + (1.0f - nDotH) * (1.0f + nDotH);
    return a2 / (PI * denom * denom);
}

float geometrySchlickGgx(float nDotV, float roughness) {
    const float r = roughness + 1.0f;
    const float k = r * r / 8.0f;
    return nDotV / (nDotV * (1.0f - k) + k);
}

float geometrySmith(float nDotV, float nDotL, float roughness) {
    return geometrySchlickGgx(nDotV, roughness) * geometrySchlickGgx(nDotL, roughness);
}

// Generalized GTR.
float distributionGgxAniso(float nDotH, float hDotT, float hDotB, float alphaU, float alphaV) {
    const float alphaU2 = alphaU * alphaU;
    const float alphaV2 = alphaV * alphaV;

    const float nDotH2 = nDotH * nDotH;
    const float hDotT2 = hDotT * hDotT;
    const float hDotB2 = hDotB * hDotB;

    const float numerator = nDotH > 0.0f ? 1.0f : 0.0f;
    const float denominator = hDotT2 / alphaU2 + hDotB2 / alphaV2 + nDotH2;

    return numerator / (PI * alphaU * alphaV * denominator * denominator);
}

float ggxLambdaAnisotropic(float xDotN, float xDotT, float xDotB, float alphaU, float alphaV) {
    const float alphaTerm =
        (xDotT * xDotT * alphaU * alphaU + xDotB * xDotB * alphaV * alphaV) / (xDotN * xDotN);
    return (-1.0f + sqrt(1.0f + alphaTerm)) * 0.5f;
}

float geometrySchlickGgxAniso(
    float lDotH,
    float vDotH,
    float nDotV,
    float vDotT,
    float vDotB,
    float nDotL,
    float tDotL,
    float bDotL,
    float alphaU,
    float alphaV) {
    const float numeratorL = lDotH > 0.0f ? 1.0f : 0.0f;
    const float numeratorV = vDotH > 0.0f ? 1.0f : 0.0f;

    const float lambdaV = ggxLambdaAnisotropic(nDotV, vDotT, vDotB, alphaU, alphaV);
    const float lambdaL = ggxLambdaAnisotropic(nDotL, tDotL, bDotL, alphaU, alphaV);

    return numeratorL * numeratorV / (1.0f + lambdaV + lambdaL);
}

vec3 sampleGgxNormal(vec2 unitSample, float alpha) {
    const float tanThetaSquared = alpha * alpha * unitSample.y / (1.0f - unitSample.y);
    const float cosTheta = 1.0f / sqrt(1.0f + tanThetaSquared);
    const float phi = 2.0f * PI * unitSample.x;
    const float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

float ggxDistribution(vec3 normal, float alpha) {
    return normal.z > 0.0f ? distributionGgx(normal.z, alpha) : 0.0f;
}

float ggxSmithG1(vec3 v, vec3 microfacetNormal, float alpha) {
    if (dot(v, microfacetNormal) * v.z <= 0.0f) {
        return 0.0f;
    }

    const float absTanTheta = abs(microfacetTanTheta(v));
    if (absTanTheta == 0.0f) {
        return 1.0f;
    }

    const float a = alpha * absTanTheta;
    return 2.0f / (1.0f + sqrt(1.0f + a * a));
}

float ggxGeometry(vec3 wi, vec3 wo, vec3 microfacetNormal, float alpha) {
    return ggxSmithG1(wi, microfacetNormal, alpha) * ggxSmithG1(wo, microfacetNormal, alpha);
}

float computeGgxNormalPdf(vec3 microfacetNormal, float alpha) {
    return ggxDistribution(microfacetNormal, alpha) * abs(microfacetNormal.z);
}

#endif // CRISP_MICROFACET_GGX_GLSL
