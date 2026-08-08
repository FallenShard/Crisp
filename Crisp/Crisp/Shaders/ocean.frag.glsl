#version 450 core

#extension GL_GOOGLE_include_directive : require

#define PI 3.1415926535897932384626433832795
layout(location = 0) out vec4 finalColor;

layout(location = 0) in vec3 eyePosition;
layout(location = 1) in vec3 eyeNormal;
layout(location = 2) in vec3 worldNormal;

#include "Brdf/microfacet.part.glsl"
#include "Common/view.part.glsl"

// ----- Camera -----
layout(set = 1, binding = 0) uniform View {
    ViewParameters view;
};

layout (set = 1, binding = 1) uniform samplerCube irrMap;
layout (set = 1, binding = 2) uniform samplerCube refMap;
layout (set = 1, binding = 3) uniform sampler2D brdfLut;

const float kMaxReflectionLod = 4.0f;

vec3 computeEnvRadiance(vec3 eyeN, vec3 eyeV, vec3 albedo, float roughness, out vec3 reflection)
{
    const vec3 worldN = (view.invV * vec4(eyeN, 0.0f)).rgb;
    const vec3 irradiance = texture(irrMap, worldN).rgb;
    const vec3 ambient = irradiance * albedo * 0.5f;

    const vec3 eyeR = reflect(-eyeV, eyeN);
    const vec3 worldR = (view.invV * vec4(eyeR, 0.0f)).rgb;
    reflection = textureLod(refMap, worldR, roughness * kMaxReflectionLod).rgb;

    return ambient;
}

void main()
{
    const vec3 eyeN = normalize(eyeNormal);
    const vec3 eyeV = normalize(-eyePosition);
    const float NdotV = max(dot(eyeN, eyeV), 0.0f);

    const vec3 lightDir = normalize(vec3(1.0f, 1.0f, 1.0f));
    const vec3 eyeL = normalize(vec3(view.V * vec4(lightDir, 0.0f)));
    const float NdotL = max(dot(eyeN, eyeL), 0.0f);

    const float roughness = 0.045f;
    const vec3 F = fresnelSchlick(NdotV, vec3(0.02f));

    const vec3 eyeH = normalize(eyeL + eyeV);
    const float NdotH = max(dot(eyeN, eyeH), 0.0f);
    const float D = distributionGGX(NdotH, roughness);
    const float G = geometrySmith(NdotV, NdotL, roughness);
    const vec3 sunGlitter = (D * G * F / max(4.0f * NdotV * NdotL, 0.001f)) * NdotL;

    const vec3 waterAlbedo = pow(vec3(12, 120, 167) / 255.0f, vec3(2.2f));
    vec3 reflection;
    const vec3 ambient = computeEnvRadiance(eyeN, eyeV, waterAlbedo, roughness, reflection);

    const float waveHeight = (view.invV * vec4(eyePosition, 1.0f)).y;
    const float crestFactor = smoothstep(-1.0f, 1.0f, waveHeight);
    const float backLitFactor = pow(max(dot(eyeV, -eyeL), 0.0f), 4.0f);
    const vec3 subsurfaceColor = pow(vec3(20, 140, 130) / 255.0f, vec3(2.2f));
    const vec3 subsurface = subsurfaceColor * crestFactor * backLitFactor;

    const vec3 scatterColor = ambient + subsurface;
    finalColor = vec4(mix(scatterColor, reflection, F) + sunGlitter, 1.0f);
}
