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
    vec3 albedo;
    float metallic;
    float roughness;
} mat;

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
layout(set = 1, binding = 0) buffer PointLights
{
    LightDescriptor pointLights[];
};

layout(set = 1, binding = 1) buffer LightIndexList
{
    uint lightIndexList[];
};

layout(set = 3, binding = 0, rg32ui) uniform readonly uimage2D lightGrid;

vec3 evalPointLightRadiance(in vec3 lightPos, in vec3 spectrum, in float radius, out vec3 eyeL)
{
    const vec3 eyeLightPos = (V * vec4(lightPos, 1.0f)).xyz;
    const vec3 eyePosToLight = eyeLightPos - eyePosition;
    float dist = length(eyePosToLight);
    eyeL = eyePosToLight / dist;

    float fw = pow(max(0.0f, 1.0f - pow(dist / 10.0f, 4.0f)), 2.0f);

    return spectrum * fw / (dist * dist);
}

void main()
{
    // Basic shading geometry
    const vec3 eyeN = normalize(eyeNormal);
    //const vec3 eyeV = normalize(-eyePosition);
    //const float NdotV = max(dot(eyeN, eyeV), 0.0f);

    // Primary light source radiance
    //vec3 eyeL; // light direction, out param
    //const vec3 Le = evalPointLightRadiance(eyeL);
    //const float NdotL = max(dot(eyeN, eyeL), 0.0f);
//
    //// Material properties
    //const vec3  albedo    = mat.albedo;
    //const float roughness = mat.roughness;
    //const float metallic  = mat.metallic;
    //const float ao        = 1.0f;
//
    //// BRDF diffuse (Light source independent)
    //const vec3 F0 = mix(vec3(0.04), albedo, metallic);
    //const vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    //const vec3 kS = F;
    //const vec3 kD = (1.0f - kS) * (1.0f - metallic);
    //const vec3 diffuse = kD * albedo / PI;
//
    //// BRDF specularity (Light source dependent)
    //const vec3 eyeH = normalize(eyeL + eyeV);
    //const float D = distributionGGX(eyeN, eyeH, roughness);
    //const float G = geometrySmith(NdotV, NdotL, roughness);
    //const vec3 specularity = D * G * F / max(4.0f * NdotV * NdotL, 0.001);
//
    //const vec3 Li = (diffuse + specularity) * Le * NdotL;
    //const vec3 Lenv = computeEnvRadiance(eyeN, eyeV, kD, albedo, F, roughness, ao);
    //
//
    //float bias = 0.01f * tan(acos(NdotL));
    ////bias = clamp(bias, 0.0f, 0.01f);
    ////const float shadow = getShadowCoeff(bias, pointLight.VP[0], 3);
    ////const float shadow = getVsmCoeff();
//
    //vec3 debugColor;
    //const float shadow = getCsmShadowCoeffDebug(, debugColor);

    vec3 acc = vec3(0.0f);

    vec3 eyeL;
    //for (int i = 0; i < 1024; ++i)
    //{
    //    vec3 Le = evalPointLightRadiance(pointLights[i].position.xyz, pointLights[i].spectrum.xyz, eyeL);
    //    acc += max(dot(eyeN, eyeL), 0.0f) * Le;
    //}

    uvec2 lightIndexData = uvec2(imageLoad(lightGrid, ivec2(gl_FragCoord.xy) / ivec2(16)).xy);
    for (uint i = 0; i < lightIndexData[1]; ++i)
    {
        uint idx = lightIndexList[lightIndexData[0] + i];
        vec3 Le = evalPointLightRadiance(pointLights[idx].position.xyz, pointLights[idx].spectrum.xyz, pointLights[idx].params.r, eyeL);
        acc += max(dot(eyeN, eyeL), 0.0f) * Le;
    }


    //vec3 Le = evalPointLightRadiance(pointLights[0].position.xyz, pointLights[0].spectrum.xyz, pointLights[0].params.r, eyeL);
    //acc = max(dot(eyeN, eyeL), 0.0f) * Le;
//
//
//
    //fragColor = vec4(Lenv * 0.0f + Li * getVsmCoeff(), 1.0f);

    fragColor = vec4(acc, 1.0f);
    //fragColor = vec4(vec3(shadow), 1.0f);
}