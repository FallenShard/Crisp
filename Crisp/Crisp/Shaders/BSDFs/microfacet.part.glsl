#ifndef CRISP_MICROFACET_GLSL
#define CRISP_MICROFACET_GLSL

#include "fresnel.part.glsl"
#include "../Common/math-constants.part.glsl"
#include "../Common/warp.part.glsl"

const int kMicrofacetGgx = 0;
const int kMicrofacetBeckmann = 1;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

float distributionGgx(float NdotH, float alpha) {
    const float a2 = max(alpha * alpha, 1e-12f);
    const float denom = a2 * NdotH * NdotH + (1.0f - NdotH) * (1.0f + NdotH);
    return a2 / (PI * denom * denom);
}

float geometrySchlickGgx(float NdotV, float roughness) {
    const float r = roughness + 1.0f;
    const float k = r * r / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float geometrySmith(float NdotV, float NdotL, float roughness) {
    return geometrySchlickGgx(NdotV, roughness) * geometrySchlickGgx(NdotL, roughness);
}

// Generalized GTR
float distributionGgxAniso(float NdotH, float HdotT, float HdotB, float alphaU, float alphaV) {
    const float alphaU2 = alphaU * alphaU;
    const float alphaV2 = alphaV * alphaV;

    const float NdotH2 = NdotH * NdotH;
    const float HdotT2 = HdotT * HdotT;
    const float HdotB2 = HdotB * HdotB;

    const float num = NdotH > 0.0f ? 1 : 0.0f;
    const float denom = HdotT2 / alphaU2 + HdotB2 / alphaV2 + NdotH2;

    return num / (PI * alphaU * alphaV * denom * denom);
}

float lambda(float XdotN, float XdotT, float XdotB, float alphaU, float alphaV) {
    const float alphaTerm = (XdotT * XdotT * alphaU * alphaU + XdotB * XdotB * alphaV * alphaV) / (XdotN * XdotN);
    return (-1.0f + sqrt(1 + alphaTerm)) * 0.5f;
}

float geometrySchlickGgxAniso(
    float LdotH,
    float VdotH,
    float NdotV,
    float VdotT,
    float VdotB,
    float NdotL,
    float TdotL,
    float BdotL,
    float alphaU,
    float alphaV) {
    const float num1 = LdotH > 0.0f ? 1.0f : 0.0f;
    const float num2 = VdotH > 0.0f ? 1.0f : 0.0f;

    const float lambdaV = lambda(NdotV, VdotT, VdotB, alphaU, alphaV);
    const float lambdaL = lambda(NdotL, TdotL, BdotL, alphaU, alphaV);

    return num1 * num2 / (1.0f + lambdaV + lambdaL);
}

vec3 sampleGgxNormal(vec2 unitSample, float alpha) {
    const float tanThetaSquared = alpha * alpha * unitSample.y / (1.0f - unitSample.y);
    const float cosTheta = 1.0f / sqrt(1.0f + tanThetaSquared);
    const float phi = 2.0f * PI * unitSample.x;
    const float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

vec3 sampleBeckmannNormal(vec2 unitSample, float alpha) {
    const float tanThetaSquared = -alpha * alpha * log(max(1.0f - unitSample.y, 1e-7f));
    const float cosTheta = inversesqrt(1.0f + tanThetaSquared);
    const float phi = 2.0f * PI * unitSample.x;
    const float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

float ggxDistribution(vec3 normal, float alpha) {
    return normal.z > 0.0f ? distributionGgx(normal.z, alpha) : 0.0f;
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

float ggxTanTheta(vec3 v) {
    const float sinThetaSquared = 1.0f - v.z * v.z;
    if (sinThetaSquared <= 0.0f) {
        return 0.0f;
    }

    return sqrt(sinThetaSquared) / v.z;
}

float ggxSmithG1(vec3 v, vec3 microfacetNormal, float alpha) {
    if (dot(v, microfacetNormal) * v.z <= 0.0f) {
        return 0.0f;
    }

    const float absTanTheta = abs(ggxTanTheta(v));
    if (absTanTheta == 0.0f) {
        return 1.0f;
    }

    const float a = alpha * absTanTheta;
    return 2.0f / (1.0f + sqrt(1.0f + a * a));
}

float beckmannSmithG1(vec3 v, vec3 microfacetNormal, float alpha) {
    if (dot(v, microfacetNormal) * v.z <= 0.0f) {
        return 0.0f;
    }

    const float absTanTheta = abs(ggxTanTheta(v));
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

float ggxGeometry(vec3 wi, vec3 wo, vec3 microfacetNormal, float alpha) {
    return ggxSmithG1(wi, microfacetNormal, alpha) * ggxSmithG1(wo, microfacetNormal, alpha);
}

float beckmannGeometry(vec3 wi, vec3 wo, vec3 microfacetNormal, float alpha) {
    return beckmannSmithG1(wi, microfacetNormal, alpha) * beckmannSmithG1(wo, microfacetNormal, alpha);
}

vec3 sampleMicrofacetNormal(vec2 unitSample, int microfacetType, float alpha) {
    return microfacetType == kMicrofacetBeckmann
        ? sampleBeckmannNormal(unitSample, alpha)
        : sampleGgxNormal(unitSample, alpha);
}

vec3 sampleMicrofacet(
    vec2 unitSample, vec3 wi, float specularProbability, int microfacetType, float alpha, out bool sampledSpecular) {
    sampledSpecular = unitSample.x < specularProbability;
    if (sampledSpecular) {
        unitSample.x /= specularProbability;
        const vec3 microfacetNormal = sampleMicrofacetNormal(unitSample, microfacetType, alpha);
        return 2.0f * dot(microfacetNormal, wi) * microfacetNormal - wi;
    }

    unitSample.x = (unitSample.x - specularProbability) / (1.0f - specularProbability);
    return squareToCosineHemisphere(unitSample);
}

float microfacetDistribution(vec3 normal, int microfacetType, float alpha) {
    return microfacetType == kMicrofacetBeckmann
        ? beckmannDistribution(normal, alpha)
        : ggxDistribution(normal, alpha);
}

float microfacetGeometry(vec3 wi, vec3 wo, vec3 microfacetNormal, int microfacetType, float alpha) {
    return microfacetType == kMicrofacetBeckmann
        ? beckmannGeometry(wi, wo, microfacetNormal, alpha)
        : ggxGeometry(wi, wo, microfacetNormal, alpha);
}

float computeMicrofacetNormalPdf(vec3 microfacetNormal, int microfacetType, float alpha) {
    return microfacetDistribution(microfacetNormal, microfacetType, alpha) * abs(microfacetNormal.z);
}

float computeMicrofacetPdf(vec3 wi, vec3 wo, float ks, int microfacetType, float alpha) {
    if (wi.z <= 0.0f || wo.z <= 0.0f) {
        return 0.0f;
    }

    const float diffusePdf = wo.z / PI;
    const vec3 microfacetNormal = normalize(wi + wo);
    const float halfVectorJacobian = 1.0f / (4.0f * dot(microfacetNormal, wo));
    const float specularPdf = computeMicrofacetNormalPdf(microfacetNormal, microfacetType, alpha) * halfVectorJacobian;

    return mix(diffusePdf, specularPdf, ks);
}

vec3 evaluateMicrofacet(
    vec3 kd, float ks, float extIor, float intIor, int microfacetType, float alpha, vec3 wi, vec3 wo) {
    const float cosThetaI = wi.z;
    const float cosThetaO = wo.z;
    if (cosThetaI <= 0.0f || cosThetaO <= 0.0f) {
        return vec3(0.0f);
    }

    const vec3 microfacetNormal = normalize(wi + wo);
    const vec3 diffuse = kd / PI;

    float cosThetaT;
    const float fresnel = fresnelDielectric(dot(wi, microfacetNormal), extIor, intIor, cosThetaT);
    const float distribution = microfacetDistribution(microfacetNormal, microfacetType, alpha);
    const float geometry = microfacetGeometry(wi, wo, microfacetNormal, microfacetType, alpha);
    const vec3 specular = vec3(ks * fresnel * distribution * geometry);

    return diffuse * cosThetaO + specular / (4.0f * cosThetaI);
}

#endif // CRISP_MICROFACET_GLSL
