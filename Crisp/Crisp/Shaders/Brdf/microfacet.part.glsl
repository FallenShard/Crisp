#ifndef CRISP_MICROFACET_GLSL
#define CRISP_MICROFACET_GLSL

#include "dielectric.part.glsl"
#include "../Common/math-constants.part.glsl"

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

float distributionGGX(float NdotH, float alpha) {
    const float a2 = alpha * alpha;
    const float denom = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * denom * denom);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    const float r = roughness + 1.0f;
    const float k = r * r / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float geometrySmith(float NdotV, float NdotL, float roughness) {
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

// Generalized GTR
float distributionGGXAniso(float NdotH, float HdotT, float HdotB, float alphaU, float alphaV) {
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

float geometrySchlickGGXAniso(
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

vec3 sampleGGXNormal(vec2 unitSample, float alpha) {
    const float tanThetaSquared = alpha * alpha * unitSample.y / (1.0f - unitSample.y);
    const float cosTheta = 1.0f / sqrt(1.0f + tanThetaSquared);
    const float phi = 2.0f * PI * unitSample.x;
    const float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

float ggxDistribution(vec3 normal, float alpha) {
    return normal.z > 0.0f ? distributionGGX(normal.z, alpha) : 0.0f;
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

float ggxGeometry(vec3 wi, vec3 wo, vec3 microfacetNormal, float alpha) {
    return ggxSmithG1(wi, microfacetNormal, alpha) * ggxSmithG1(wo, microfacetNormal, alpha);
}

float ggxNormalPdf(vec3 microfacetNormal, float alpha) {
    return ggxDistribution(microfacetNormal, alpha) * abs(microfacetNormal.z);
}

float microfacetPdf(vec3 wi, vec3 wo, float ks, float alpha) {
    if (wi.z <= 0.0f || wo.z <= 0.0f) {
        return 0.0f;
    }

    const float diffusePdf = wo.z / PI;
    const vec3 microfacetNormal = normalize(wi + wo);
    const float halfVectorJacobian = 1.0f / (4.0f * dot(microfacetNormal, wo));
    const float specularPdf = ggxNormalPdf(microfacetNormal, alpha) * halfVectorJacobian;

    return mix(diffusePdf, specularPdf, ks);
}

vec3 evaluateMicrofacet(vec3 kd, float ks, float extIor, float intIor, float alpha, vec3 wi, vec3 wo) {
    const float cosThetaI = wi.z;
    const float cosThetaO = wo.z;
    if (cosThetaI <= 0.0f || cosThetaO <= 0.0f) {
        return vec3(0.0f);
    }

    const vec3 microfacetNormal = normalize(wi + wo);
    const vec3 diffuse = kd / PI;

    float cosThetaT;
    const float fresnel = fresnelDielectric(dot(wi, microfacetNormal), extIor, intIor, cosThetaT);
    const float distribution = ggxDistribution(microfacetNormal, alpha);
    const float geometry = ggxGeometry(wi, wo, microfacetNormal, alpha);
    const vec3 specular = vec3(ks * fresnel * distribution * geometry);

    return diffuse * cosThetaO + specular / (4.0f * cosThetaI);
}

#endif // CRISP_MICROFACET_GLSL
