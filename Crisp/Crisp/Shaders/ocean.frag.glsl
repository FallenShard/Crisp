#version 450 core
#define PI 3.1415926535897932384626433832795
layout(location = 0) out vec4 finalColor;

layout(location = 0) in vec3 eyePosition;
layout(location = 1) in vec3 eyeNormal;
layout(location = 2) in vec3 worldNormal;

#include "Parts/microfacet.part.glsl"

// ----- Camera -----
layout(set = 1, binding = 0) uniform Camera
{
    mat4 V;
    mat4 P;
    vec2 screenSize;
    vec2 nearFar;
};


layout (set = 1, binding = 1) uniform samplerCube irrMap;
layout (set = 1, binding = 2) uniform samplerCube refMap;
layout (set = 1, binding = 3) uniform sampler2D brdfLut;

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

void main()
{
    // Basic shading geometry
    const vec3 eyeN = normalize(eyeNormal);
    const vec3 eyeV = normalize(-eyePosition);
    const float NdotV = max(dot(eyeN, eyeV), 0.0f);

    // Primary light source radiance
    const vec3 lightDir = normalize(vec3(1.0f, 1.0f, 1.0f));
    const vec3 eyeL = normalize(vec3(V * vec4(lightDir, 0.0f)));
    const vec3 Le = vec3(1.0f);
    const float NdotL = max(dot(eyeN, eyeL), 0.0f);

    // Material properties
    const vec3 albedo = pow(vec3(12,55,87) / 255.0, vec3(2.2));
    float roughness = 0.01;
    const float metallic = 0.0;

    // BRDF diffuse (Light source independent)
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);
    const vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    const vec3 kS = F;
    const vec3 kD = (1.0f - kS) * (1.0f - metallic);
    const vec3 diffuse = kD * albedo / PI;

    // BRDF specularity (Light source dependent)
    const vec3 eyeH = normalize(eyeL + eyeV);
    const float NdotH = max(dot(eyeN, eyeH), 0.0f);
    const float D = distributionGGX(NdotH, roughness);
    const float G = geometrySmith(NdotV, NdotL, roughness);
    const vec3 specularity = D * G * F / max(4.0f * NdotV * NdotL, 0.001);

    // const vec3 eyeN = normalize(eyeNormal);
    // const vec3 eyeV = normalize(-eyePosition);
    // const float NdotV = max(dot(eyeN, eyeV), 0.0f);

    // const vec3 lightDir = normalize(vec3(-1.0f, -1.0f, -1.0f));
    // const vec3 eyeL = normalize(vec3(V * vec4(lightDir, 0.0f)));
    // const float NdotL = max(dot(eyeN, -eyeL), 0.0f);

    // vec3 ocean_color = pow(vec3(0,55,87) / 255.0, vec3(2.2));
    // // Material properties
    // const vec3  albedo    = ocean_color;
    // const float roughness = 0.5;
    // const float metallic  = 0.2;

    // const vec3 F0 = mix(vec3(0.02), albedo, metallic);
    // const vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    // const vec3 kS = F;
    // const vec3 kD = (1.0f - kS) * (1.0f - metallic);
    // const vec3 diffuse = kD * albedo / PI;

   
    //vec3 light_color = pow(vec3(185,240,254) / 255.0, vec3(2.2));
    //vec3 color = mix(ocean_color, light_color, fresnel * fresnel);////

    const vec3 worldN = normalize(worldNormal);

    //const vec3 Le = vec3(5.0f);

    const vec3 Li = (diffuse + specularity) * Le * NdotL;
    const vec3 Lenv = computeEnvRadiance(eyeN, eyeV, kD, albedo, F, 0.0f, 1.0f);
    
    finalColor = vec4(vec3(Lenv), 1.0f);
    //finalColor = vec4(vec3(F), 1.0f);
    //finalColor = vec4(vec3(Li), 1.0f);
}
