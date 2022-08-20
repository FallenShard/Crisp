#version 450 core

#define PI 3.1415926535897932384626433832795

#include "Parts/microfacet.part.glsl"

const vec3 NdcMin = vec3(-1.0f, -1.0f, 0.0f);
const vec3 NdcMax = vec3(+1.0f, +1.0f, 1.0f);

layout(location = 0) in vec3 eyeNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 eyePosition;
layout(location = 3) in vec3 eyeTangent;
layout(location = 4) in vec3 eyeBitangent;
layout(location = 5) in vec3 worldPos;

layout(location = 0) out vec4 fragColor;

// ----- Camera -----
layout(set = 1, binding = 0) uniform Camera
{
    mat4 V;
    mat4 P;
    vec2 screenSize;
    vec2 nearFar;
};

// ----- Lights and Shadows -----
struct LightDescriptor
{
    mat4 V;
    mat4 P;
    mat4 VP;
    vec4 position;
    vec4 direction;
    vec4 spectrum;
    vec4 params;
};

// ----- Directional Light -----
layout(set = 1, binding = 1) uniform CascadedLight
{
    LightDescriptor cascadedLight[4];
};

layout(set = 1, binding = 2) uniform sampler2DArray cascadedShadowMapArray;

// ----- Environment Lighting -----
layout(set = 1, binding = 3) uniform samplerCube irrMap;
layout(set = 1, binding = 4) uniform samplerCube refMap;
layout(set = 1, binding = 5) uniform sampler2D brdfLut;

// ----- Material -----
layout(set = 2, binding = 0) uniform sampler2D diffuseTex;
layout(set = 2, binding = 1) uniform sampler2D metallicTex;
layout(set = 2, binding = 2) uniform sampler2D roughnessTex;
layout(set = 2, binding = 3) uniform sampler2D normalTex;
layout(set = 2, binding = 4) uniform sampler2D aoTex;
layout(set = 2, binding = 5) uniform sampler2D emissiveTex;
layout(set = 2, binding = 6) uniform Material
{
    vec4 albedo;
    vec2 uvScale;
    float metallic;
    float roughness;
} material;

vec3 evalDirectionalLightRadiance(out vec3 eyeL)
{
    eyeL = normalize((V * cascadedLight[0].direction).xyz);
    return cascadedLight[0].spectrum.rgb * 2;
}

// ----- Cascaded Shadow Mapping
bool isInCascade(vec3 worldPos, mat4 lightVP)
{
    vec4 lightSpacePos = lightVP * vec4(worldPos, 1.0f);
    vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;

    return all(greaterThan(ndcPos, NdcMin)) && all(lessThan(ndcPos, NdcMax));
}

// Check-in-bounds based
float evalCascadedShadow(vec3 worldPos, float bias)
{
    int cascadeIndex = 3;
    if (isInCascade(worldPos, cascadedLight[0].VP))
       cascadeIndex = 0;
    else if (isInCascade(worldPos, cascadedLight[1].VP))
       cascadeIndex = 1;
    else if (isInCascade(worldPos, cascadedLight[2].VP))
       cascadeIndex = 2;

    vec4 lightSpacePos = cascadedLight[cascadeIndex].VP * vec4(worldPos, 1.0f);
    vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;

    if (any(lessThan(ndcPos, NdcMin)) || any(greaterThan(ndcPos, NdcMax)))
        return 1.0f;

    vec3 texCoord = vec3(ndcPos.xy * 0.5f + 0.5f, cascadeIndex);

    ivec2 size = textureSize(cascadedShadowMapArray, 0).xy;
    vec2 texelSize = vec2(1) / size;

    const int pcfRadius = 5;
    const float numSamples = (2 * pcfRadius + 1) * (2 * pcfRadius + 1);

    float amount  = 0.0f;
    for (int i = -pcfRadius; i <= pcfRadius; i++)
    {
        for (int j = -pcfRadius; j <= pcfRadius; j++)
        {
            vec2 tc = texCoord.xy + vec2(i, j) * texelSize;
            float shadowMapDepth = texture(cascadedShadowMapArray, vec3(tc, cascadeIndex)).r;
            amount += shadowMapDepth < (lightSpacePos.z - bias) / lightSpacePos.w ? 0.0f : 1.0f;
        }
    }

    return amount / numSamples;
}

vec3 computeEnvRadiance(vec3 eyeN, vec3 eyeV, vec3 kD, vec3 albedo, vec3 F, float roughness, float ao)
{
    const vec3 worldN = (inverse(V) * vec4(eyeN, 0.0f)).rgb;
    const vec3 irradiance = texture(irrMap, worldN).rgb;
    const vec3 diffuse = irradiance * albedo;

    const float NdotV = max(dot(eyeN, eyeV), 0.0f);
    const vec3 eyeR = reflect(-eyeV, eyeN);
    const vec3 worldR = (inverse(V) * vec4(eyeR, 0.0f)).rgb;

    const float MaxReflectionLod = 4.0f;
    const vec3 prefilter = textureLod(refMap, worldR, roughness * MaxReflectionLod).rgb;
    const vec2 brdf = texture(brdfLut, vec2(NdotV, roughness)).xy;
    const vec3 specular = prefilter * (F * brdf.x + brdf.y);

    return (kD * diffuse + specular) * ao;
}

vec3 decodeNormal(in vec2 uv)
{
    vec3 normal  = normalize(eyeNormal);
    vec3 tangent = normalize(eyeTangent);
    vec3 bitangent = cross(normal, tangent);//normalize(eyeBitangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    vec3 n = texture(normalTex, uv).xyz;
    return normalize(TBN * normalize(n * 2.0f - 1.0f));
}

void main()
{
    const vec2 uvCoord = inTexCoord * material.uvScale;
    
    // Basic shading geometry
    const vec3 eyeN = decodeNormal(uvCoord);
    const vec3 eyeV = normalize(-eyePosition);
    const float NdotV = max(dot(eyeN, eyeV), 0.0f);

    // Primary light source radiance
    vec3 eyeL; // light direction, out param
    const vec3 Le = evalDirectionalLightRadiance(eyeL);
    const float NdotL = max(dot(eyeN, eyeL), 0.0f);

    // Material properties
    const vec3 albedo = texture(diffuseTex, uvCoord).rgb * material.albedo.rgb;
    const float roughness = texture(roughnessTex, uvCoord).r * material.roughness;
    const float metallic = texture(metallicTex, uvCoord).r * material.metallic;
    const float ao = texture(aoTex, uvCoord).r;
    const vec3 emission = texture(emissiveTex, uvCoord).rgb;

    // BRDF diffuse (Light source independent)
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);
    const vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    const vec3 kS = F;
    const vec3 kD = (1.0f - kS) * (1.0f - metallic);
    const vec3 diffuse = kD * albedo / PI;

    // BRDF specularity (Light source dependent)
    const float alpha = roughness * roughness;
    const vec3 eyeH = normalize(eyeL + eyeV);
    const float NdotH = max(dot(eyeN, eyeH), 0.0f);
    const float D = distributionGGX(NdotH, roughness);
    const float G = geometrySmith(NdotV, NdotL, roughness);
    const vec3 specularity = D * G * F / max(4.0f * NdotV * NdotL, 0.001);

    float shadowCoeff = evalCascadedShadow(worldPos, 0.005f);
    //shadowCoeff = 0.0f;

    const vec3 Li = (diffuse + specularity) * Le * NdotL;
    

    const vec3 Lenv = computeEnvRadiance(eyeN, eyeV, kD, albedo, F, roughness, ao);

    fragColor = vec4(Lenv + Li * shadowCoeff + emission, 1.0f);
    //fragColor = vec4(vec3(ao), 1.0f);
    //fragColor = vec4(Lenv, 1.0f);
    //fragColor = vec4(Li, 1.0f);
    //fragColor = vec4(Lenv * 0.5f + vec3(shadowCoeff), 1.0f);
    //fragColor = vec4(inTexCoord, 0.0f, 1.0f);
    //fragColor = vec4(mod(uvCoord, vec2(1.0)), 0, 1);
    //fragColor = vec4(worldPos, 1.0);
    //fragColor = vec4(Lenv, 1.0f);
    //fragColor = vec4(vec3(G), 1.0f);
    //fragColor = vec4(1, 0, 0, 1);
}