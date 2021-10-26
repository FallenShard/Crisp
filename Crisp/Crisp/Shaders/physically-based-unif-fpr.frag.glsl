#version 460 core
#define PI 3.1415926535897932384626433832795
const vec3 NdcMin = vec3(-1.0f, -1.0f, 0.0f);
const vec3 NdcMax = vec3(+1.0f, +1.0f, 1.0f);

layout(location = 0) in vec3 eyeNormal;
layout(location = 1) in vec3 eyePosition;
layout(location = 2) in vec3 worldPos;

layout(location = 0) out vec4 fragColor;

// ----- Camera -----
layout(set = 0, binding = 1) uniform Camera
{
    mat4 V;
    mat4 P;
    vec2 screenSize;
    vec2 nearFar;
};

// ----- PBR Microfacet BRDF -------
layout(set = 0, binding = 2) uniform Material
{
    vec4 albedo;
    float metallic;
    float roughness;
} material;

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float num    = a2;
    float denom  = NdotH2 * (a2 - 1.0f) + 1.0f;
    denom        = PI * denom * denom;

    return num / denom;
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = r * r / 8.0f;

    float num   = NdotV;
    float denom = NdotV * (1.0f - k) + k;

    return num / denom;
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

// ----- Generic Light Descriptor of 256 bytes -----
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

void createCoordSystem(in vec3 v1, out vec3 v2, out vec3 v3)
{
    if (abs(v1.x) > abs(v1.y))
    {
        float invLen = 1.0f / sqrt(v1.x * v1.x + v1.z * v1.z);
        v3 = vec3(v1.z * invLen, 0.0f, -v1.x * invLen);
    }
    else
    {
        float invLen = 1.0f / sqrt(v1.y * v1.y + v1.z * v1.z);
        v3 = vec3(0.0f, v1.z * invLen, -v1.y * invLen);
    }
    v2 = cross(v3, v1);
}

vec3 toLocal(vec3 v, vec3 n, vec3 s, vec3 t)
{
    return vec3(dot(v, s), dot(v, t), dot(v, n));
}

vec3 evalPolygonalLight(vec3 n)
{
    vec3 s;
    vec3 t;
    createCoordSystem(n, s, t);
    
    vec3 pos[4];
    pos[0] = vec3(-1.0f, 3.0f, +1.0f);
    pos[1] = vec3(+1.0f, 3.0f, -1.0f);
    pos[2] = vec3(+1.0f, 3.0f, +1.0f);
    pos[3] = vec3(-1.0f, 3.0f, +1.0f);

    vec3 worldP = worldPos;

    vec3 L[4];
    L[0] = normalize(toLocal(pos[0] - worldPos, n, s, t));
    L[1] = normalize(toLocal(pos[1] - worldPos, n, s, t));
    L[2] = normalize(toLocal(pos[2] - worldPos, n, s, t));
    L[3] = normalize(toLocal(pos[3] - worldPos, n, s, t));

    vec3 irradiance = vec3(0.0f);
    for (int i = 0; i < 4; ++i)
    {
        vec3 pi = L[i];
        vec3 pj = L[(i + 1) % 4];
        float theta = acos(dot(pi, pj));
        vec3 v = normalize(cross(pi, pj));

        irradiance += 1.0f / (2.0f * PI) * theta * dot(v, vec3(0.0f, 0.0f, 1.0f));

    }

    return irradiance;
}

// vec3 evalSpotLightRadiance()
// {
//     vec3 eyeO = (V * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
//     vec3 eyeL = (V * vec4(light.position.xyz, 1.0f)).xyz;
//     vec3 coneDir = eyeO - eyeL;

//     vec3 eyeP = eyePosition;
//     vec3 lightToPos = eyeP - eyeL;

//     float sim = dot(normalize(coneDir), normalize(lightToPos));
//     if (sim < 0.7f)
//         return vec3(0.0f);
//     else if (sim < 0.8f)
//         return vec3(smoothstep(0.7, 0.8, sim));//vec3((sim - 0.7f) / 0.1f);
//     else
//         return vec3(1.0f);
// }

// vec3 evalPointLightRadiance(out vec3 eyeL)
// {
//     const vec3 eyeLightPos = (V * vec4(light.position.xyz, 1.0f)).xyz;
//     const vec3 eyePosToLight = eyeLightPos - eyePosition;
//     const float dist = length(eyePosToLight);

//     eyeL = eyePosToLight / dist;
//     return light.spectrum / (dist * dist);
// }

// vec3 evalPointLightRadiance(in vec3 lightPos, in vec3 spectrum, out vec3 eyeL)
// {
//     const vec3 eyeLightPos = (V * vec4(lightPos, 1.0f)).xyz;
//     const vec3 eyePosToLight = eyeLightPos - eyePosition;
//     const float dist = length(eyePosToLight);

//     eyeL = eyePosToLight / dist;
//     return spectrum / (dist * dist);
// }

// vec3 evalDirectionalLightRadiance(out vec3 eyeL)
// {
//     eyeL = normalize((V * vec4(light.position.xyz, 0.0f)).xyz);
//     return light.spectrum;
// }

// // ----- Cascaded Shadow Mapping -----

// layout(set = 1, binding = 1) uniform sampler2DArray shadowMap;

// layout(set = 1, binding = 2) uniform CascadeSplit
// {
//     vec4 lo;
//     vec4 hi;
// } cascadeSplit;

// layout(set = 1, binding = 3) uniform LightTransforms
// {
//     mat4 VP[4];
// } lightTransforms;

// // ----- Standard Shadow Mapping -----

// layout(set = 1, binding = 4) uniform sampler2D pointShadowMap;
// layout(set = 1, binding = 5) uniform LightTransforms2
// {
//     mat4 VP[4];
// } pointLight;

// struct ManyLightDescriptor
// {
//     vec3 position;
//     float radius;
//     vec3 spectrum;
//     float padding;
// };

// layout(set = 1, binding = 6) uniform ManyLights
// {
//     ManyLightDescriptor lights[1024];
// } many;

// layout(set = 1, binding = 7, rg32ui) uniform readonly uimage2D lightGrid;
// layout(set = 1, binding = 8) buffer LightIndexList
// {
//     uint lightIndexList[];
// };

// layout(push_constant) uniform LightTest
// {
//     layout(offset = 0) int switchMethod;
// };


void main()
{
    // Basic shading geometry
    const vec3 eyeN = normalize(eyeNormal);
    const vec3 eyeV = normalize(-eyePosition);
    const float NdotV = max(dot(eyeN, eyeV), 0.0f);

    // Primary light source radiance
    vec3 eyeL; // light direction, out param
    const vec3 Le = evalDirectionalLightRadiance(eyeL);
    const float NdotL = max(dot(eyeN, eyeL), 0.0f);

    // Material properties
    const vec3  albedo    = material.albedo.rgb;
    const float roughness = material.roughness;
    const float metallic  = material.metallic;
    const float ao        = 1.0f;

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

    const vec3 Li = (diffuse + specularity) * Le * NdotL;

    float shadowCoeff = evalCascadedShadow(worldPos, 0.005f);

    const vec3 Lenv = computeEnvRadiance(eyeN, eyeV, kD, albedo, F, roughness, ao);
    //
//
    //float bias = 0.01f * tan(acos(NdotL));
    ////bias = clamp(bias, 0.0f, 0.01f);
    ////const float shadow = getShadowCoeff(bias, pointLight.VP[0], 3);
    ////const float shadow = getVsmCoeff();
//
    //vec3 debugColor;
    ////const float shadow = getCsmShadowCoeffDebug(, debugColor);
//
    //vec3 acc = vec3(0.0f);
//
    //if (switchMethod == 0)
    //{
    //    for (int i = 0; i < 1024; ++i)
    //        {
    //        vec3 Le = evalPointLightRadiance(many.lights[i].position, many.lights[i].spectrum, eyeL);
    //        acc += max(dot(eyeN, eyeL), 0.0f) * Le;
    //        }
    //}
    //else
    //{
    //    uvec2 lightIndexData = uvec2(imageLoad(lightGrid, ivec2(gl_FragCoord.xy) / ivec2(16)).xy);
    //        for (uint i = 0; i < lightIndexData[1]; ++i)
    //        {
    //           uint idx = lightIndexList[lightIndexData[0] + i];
    //           vec3 Le = evalPointLightRadiance(many.lights[idx].position, many.lights[idx].spectrum, eyeL);
    //           acc += max(dot(eyeN, eyeL), 0.0f) * Le;
    //        }
    //}
//
    //fragColor = vec4(Lenv * 0.0f + Li * getVsmCoeff(), 1.0f);
    mat4 invV = inverse(V);
    vec3 worldN = (invV * vec4(eyeN, 0.0f)).xyz;

    vec3 areaL = evalPolygonalLight(worldN);

    fragColor = vec4(Lenv + shadowCoeff * Li, 1.0f);
    //fragColor = vec4(Lenv, 1.0f);
    //fragColor = vec4(areaL, 1.0f);
    //fragColor = vec4(shadowCoeff * Li, 1.0f);
}