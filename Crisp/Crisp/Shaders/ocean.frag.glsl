#version 450 core

#extension GL_GOOGLE_include_directive : require

#define PI 3.1415926535897932384626433832795
layout(location = 0) out vec4 finalColor;

layout(location = 0) in vec3 eyePosition;
layout(location = 1) in vec2 oceanUv;

#include "Brdf/microfacet.part.glsl"
#include "Common/view.part.glsl"

layout(push_constant) uniform PushConstant {
    layout(offset = 0) vec3 sunDirection;
    layout(offset = 12) float sunIntensity;
    layout(offset = 16) float patchWorldSize;
    layout(offset = 20) int instancesPerSide;
    layout(offset = 24) int gridSize;
    layout(offset = 28) float choppiness;
    layout(offset = 32) float waterRoughness;
    layout(offset = 36) float foamThreshold;
    layout(offset = 40) float foamSoftness;
    layout(offset = 44) float foamIntensity;
    layout(offset = 48) float invRmsWaveHeight;
};

layout(set = 0, binding = 1) uniform sampler2D packedHeightDispXMap;
layout(set = 0, binding = 2) uniform sampler2D packedDispZNormalXMap;
layout(set = 0, binding = 3) uniform sampler2D normalZMap;

layout(set = 1, binding = 0) uniform View {
    ViewParameters view;
};

layout(set = 1, binding = 1) uniform samplerCube irrMap;
layout(set = 1, binding = 2) uniform samplerCube refMap;
layout(set = 1, binding = 3) uniform sampler2D brdfLut;

// Stands in for depth-resolved transmission; see docs/ocean.md item 14.
const float kScatterFraction = 0.35f;

vec3 computeEnvSpecular(vec3 eyeN, vec3 eyeV, vec3 F0, float roughness, out vec3 irradiance) {
    const vec3 worldN = (view.invV * vec4(eyeN, 0.0f)).rgb;
    irradiance = texture(irrMap, worldN).rgb;

    const vec3 eyeR = reflect(-eyeV, eyeN);
    const vec3 worldR = (view.invV * vec4(eyeR, 0.0f)).rgb;
    const float maxReflectionLod = float(textureQueryLevels(refMap) - 1);
    const vec3 reflection = textureLod(refMap, worldR, roughness * maxReflectionLod).rgb;
    const float NdotV = max(dot(eyeN, eyeV), 0.0f);
    const vec2 brdf = texture(brdfLut, vec2(NdotV, roughness)).xy;

    return reflection * (F0 * brdf.x + brdf.y);
}

void main() {
    const vec2 texelSize = 1.0f / vec2(textureSize(packedHeightDispXMap, 0));
    const float invDerivativeSpan = 0.5f * float(gridSize) / patchWorldSize;
    const float dxLeft = texture(packedHeightDispXMap, oceanUv - vec2(texelSize.x, 0.0f)).g;
    const float dxRight = texture(packedHeightDispXMap, oceanUv + vec2(texelSize.x, 0.0f)).g;
    const float dxBack = texture(packedHeightDispXMap, oceanUv - vec2(0.0f, texelSize.y)).g;
    const float dxFront = texture(packedHeightDispXMap, oceanUv + vec2(0.0f, texelSize.y)).g;
    const float dzLeft = texture(packedDispZNormalXMap, oceanUv - vec2(texelSize.x, 0.0f)).r;
    const float dzRight = texture(packedDispZNormalXMap, oceanUv + vec2(texelSize.x, 0.0f)).r;
    const float dzBack = texture(packedDispZNormalXMap, oceanUv - vec2(0.0f, texelSize.y)).r;
    const float dzFront = texture(packedDispZNormalXMap, oceanUv + vec2(0.0f, texelSize.y)).r;

    const float dDxDx = (dxRight - dxLeft) * invDerivativeSpan;
    const float dDxDz = (dxFront - dxBack) * invDerivativeSpan;
    const float dDzDx = (dzRight - dzLeft) * invDerivativeSpan;
    const float dDzDz = (dzFront - dzBack) * invDerivativeSpan;
    const float jacobianDeterminant =
        (1.0f - choppiness * dDxDx) * (1.0f - choppiness * dDzDz) - choppiness * choppiness * dDxDz * dDzDx;
    const float compression = max(1.0f - jacobianDeterminant, 0.0f);

    const vec2 slope = vec2(texture(packedDispZNormalXMap, oceanUv).g, texture(normalZMap, oceanUv).r);

    // Choppiness shears the tangents sideways; only the Jacobian terms carry that.
    const vec3 dPdx = vec3(1.0f - choppiness * dDxDx, slope.x, -choppiness * dDzDx);
    const vec3 dPdz = vec3(-choppiness * dDxDz, slope.y, 1.0f - choppiness * dDzDz);
    const vec3 worldN = normalize(cross(dPdz, dPdx));

    const vec3 eyeN = normalize((view.V * vec4(worldN, 0.0f)).xyz);
    const vec3 eyeV = normalize(-eyePosition);
    const float NdotV = max(dot(eyeN, eyeV), 0.0f);

    const vec3 eyeL = normalize((view.V * vec4(sunDirection, 0.0f)).xyz);
    const float NdotL = max(dot(eyeN, eyeL), 0.0f);

    const float waveHeight = texture(packedHeightDispXMap, oceanUv).r;
    const float crestFactor = smoothstep(0.0f, 1.5f, waveHeight * invRmsWaveHeight);
    const float breakingFoam = smoothstep(foamThreshold, foamThreshold + foamSoftness, compression);
    const float foam = clamp(foamIntensity * breakingFoam * crestFactor, 0.0f, 1.0f);

    const float roughness = mix(max(waterRoughness, 0.03f), 0.35f, foam);
    const vec3 F0 = vec3(0.02f);
    const vec3 F = fresnelSchlick(NdotV, F0);

    const vec3 eyeH = normalize(eyeL + eyeV);
    const float NdotH = max(dot(eyeN, eyeH), 0.0f);
    const float D = distributionGGX(NdotH, roughness * roughness); // GGX takes alpha, not perceptual roughness.
    const float G = geometrySmith(NdotV, NdotL, roughness);
    const vec3 sunFresnel = fresnelSchlick(max(dot(eyeV, eyeH), 0.0f), F0);
    const vec3 sunSpecular = (D * G * sunFresnel / max(4.0f * NdotV * NdotL, 0.001f)) * NdotL * sunIntensity;

    vec3 irradiance;
    const vec3 reflection = computeEnvSpecular(eyeN, eyeV, F0, roughness, irradiance);

    // Water has no Lambertian albedo, so both scatter terms share one (1 - F).
    const vec3 scatterColor = pow(vec3(12, 120, 167) / 255.0f, vec3(2.2f));
    const vec3 subsurfaceColor = pow(vec3(20, 140, 130) / 255.0f, vec3(2.2f));
    // Sun terms need the diffuse 1/PI; the irradiance map already has it baked in.
    const float backLitFactor = pow(max(dot(eyeV, -eyeL), 0.0f), 4.0f);
    const vec3 body =
        (1.0f - F) * (scatterColor * irradiance * kScatterFraction +
                      subsurfaceColor * crestFactor * backLitFactor * sunIntensity / PI);

    const vec3 waterRadiance = body + reflection + sunSpecular;
    const vec3 foamColor = pow(vec3(235, 244, 238) / 255.0f, vec3(2.2f));
    const vec3 foamRadiance = foamColor * (irradiance + sunIntensity * NdotL / PI);
    finalColor = vec4(mix(waterRadiance, foamRadiance, foam), 1.0f);
}
