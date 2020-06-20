#version 450 core
#define PI 3.1415926535897932384626433832795
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
layout(set = 0, binding = 1) uniform Camera
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
layout(set = 1, binding = 0) uniform CascadedLight
{
    LightDescriptor cascadedLight[4];
};

layout(set = 1, binding = 1) uniform sampler2DArray cascadedShadowMapArray;

vec3 evalDirectionalLightRadiance(out vec3 eyeL)
{
    eyeL = normalize((V * cascadedLight[0].direction).xyz);
    return cascadedLight[0].spectrum.rgb;
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

    const int pcfRadius = 3;
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

// ----- Environment Lighting -----
layout (set = 2, binding = 0) uniform samplerCube irrMap;
layout (set = 2, binding = 1) uniform samplerCube refMap;
layout (set = 2, binding = 2) uniform sampler2D brdfLut;

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

// ----- Material -----
layout(set = 1, binding = 2) uniform sampler2D diffuseTex;
layout(set = 1, binding = 3) uniform sampler2D metallicTex;
layout(set = 1, binding = 4) uniform sampler2D roughnessTex;
layout(set = 1, binding = 5) uniform sampler2D normalTex;
layout(set = 1, binding = 6) uniform sampler2D aoTex;

layout (push_constant) uniform PushConstant
{
	layout(offset = 0) vec2 uvScale;
};

// -----
float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float denom  = NdotH2 * (a2 - 1.0f) + 1.0f;
    denom        = PI * denom * denom;

    return a2 / denom;
}

float geometrySchlickGGX(float NdotVec, float roughness)
{
    float r = roughness + 1.0f;
    float k = r * r / 8.0f;
    float denom = NdotVec * (1.0f - k) + k;
    return NdotVec / denom;
}

float geometrySmith(float NdotV, float NdotL, float roughness)
{
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

vec3 decodeNormal(in vec2 uv)
{
    vec3 normal  = normalize(eyeNormal);
    vec3 tangent = normalize(eyeTangent);
    vec3 bitangent = normalize(eyeBitangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    vec3 n = texture(normalTex, uv).xyz;
    n = normalize(n * 2.0f - 1.0f);
    return normalize(TBN * n);
}

void main()
{
    const vec2 uvCoord = uvScale * inTexCoord;
    
    // Basic shading geometry
    const vec3 eyeN = decodeNormal(uvCoord);
    const vec3 eyeV = normalize(-eyePosition);
    const float NdotV = max(dot(eyeN, eyeV), 0.0f);

    // Primary light source radiance
    vec3 eyeL; // light direction, out param
    const vec3 Le = evalDirectionalLightRadiance(eyeL);
    const float NdotL = max(dot(eyeN, eyeL), 0.0f);

    // Material properties
    const vec3  albedo    = texture(diffuseTex, uvCoord).rgb;
    const float roughness = texture(roughnessTex, uvCoord).r;
    const float metallic  = texture(metallicTex, uvCoord).r;
    const float ao        = texture(aoTex, uvCoord).r;

    // BRDF diffuse (Light source independent)
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);
    const vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    const vec3 kS = F;
    const vec3 kD = (1.0f - kS) * (1.0f - metallic);
    const vec3 diffuse = kD * albedo / PI;

    // BRDF specularity (Light source dependent)
    const vec3 eyeH = normalize(eyeL + eyeV);
    const float D = distributionGGX(eyeN, eyeH, roughness);
    const float G = geometrySmith(NdotV, NdotL, roughness);
    const vec3 specularity = D * G * F / max(4.0f * NdotV * NdotL, 0.001);

    float shadowCoeff = evalCascadedShadow(worldPos, 0.005f);

    const vec3 Li = (diffuse + specularity) * Le * NdotL;
    

    const vec3 Lenv = computeEnvRadiance(eyeN, eyeV, kD, albedo, F, roughness, ao);

    fragColor = vec4(Lenv + shadowCoeff * Li, 1.0f);

    //fragColor = vec4(mod(uvCoord, vec2(1.0)), 0, 1);
    //fragColor = vec4(worldPos, 1.0);
    //fragColor = vec4(Lenv, 1.0f);
    //fragColor = vec4(eyeN, 1.0f);
}