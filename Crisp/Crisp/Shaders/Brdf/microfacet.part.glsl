#ifndef MICROFACET_PART_GLSL
#define MICROFACET_PART_GLSL

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

float distributionGGX(float NdotH, float alpha)
{
    const float a2 = alpha * alpha;
    const float denom = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * denom * denom);
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    const float r = roughness + 1.0f;
    const float k = r * r / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float geometrySmith(float NdotV, float NdotL, float roughness)
{
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

// Generalized GTR
float distributionGGXAniso(float NdotH, float HdotT, float HdotB, float alphaU, float alphaV)
{
    const float alphaU2 = alphaU * alphaU;
    const float alphaV2 = alphaV * alphaV;

    const float NdotH2 = NdotH * NdotH;
    const float HdotT2 = HdotT * HdotT;
    const float HdotB2 = HdotB * HdotB;

    const float num = NdotH > 0.0f ? 1 : 0.0f;
    const float denom = HdotT2 / alphaU2 + HdotB2 / alphaV2 + NdotH2;

    return num / (PI * alphaU * alphaV * denom * denom);
}

float lambda(float XdotN, float XdotT, float XdotB, float alphaU, float alphaV)
{
    const float alphaTerm = (XdotT * XdotT * alphaU * alphaU + XdotB * XdotB * alphaV * alphaV) / (XdotN * XdotN);
    return (-1.0f + sqrt(1 + alphaTerm)) * 0.5f;
}

float geometrySchlickGGXAniso(float LdotH, float VdotH, float NdotV, float VdotT, float VdotB,
                              float NdotL, float TdotL, float BdotL, float alphaU, float alphaV)
{
    const float num1 = LdotH > 0.0f ? 1.0f : 0.0f;
    const float num2 = VdotH > 0.0f ? 1.0f : 0.0f;

    const float lambdaV = lambda(NdotV, VdotT, VdotB, alphaU, alphaV);
    const float lambdaL = lambda(NdotL, TdotL, BdotL, alphaU, alphaV);

    return num1 * num2 / (1.0f + lambdaV + lambdaL);
}

#endif 