#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "Common/math-constants.part.glsl"
#include "Common/view.part.glsl"

const int kTileSize = 16;

const float kMaxReflectionLod = 4.0f;

layout(location = 0) in vec3 eyeNormal;
layout(location = 1) in vec3 eyePosition;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 1) uniform View {
    ViewParameters view;
};

// Must match ClusteredMaterialParams in Crisp/Scenes/ClusteredLightingScene.hpp.
layout(set = 0, binding = 2) uniform Material {
    vec4 albedo;
    float metallic;
    float roughness;
    int debugMode;
}
mat;

const int kDebugModeShaded = 0;
const int kDebugModeTileHeatmap = 1;

const float kHeatmapRange = 64.0f;

vec3 heatmap(const float t) {
    const vec3 cold = vec3(0.0f, 0.0f, 0.5f);
    const vec3 mid = vec3(0.0f, 1.0f, 0.0f);
    const vec3 hot = vec3(1.0f, 0.0f, 0.0f);
    return t < 0.5f ? mix(cold, mid, t * 2.0f) : mix(mid, hot, (t - 0.5f) * 2.0f);
}

struct LightDescriptor {
    mat4 V;
    mat4 P;
    mat4 VP;
    vec4 position;
    vec4 direction;
    vec4 spectrum;
    vec4 params;
};

layout(set = 1, binding = 0) readonly buffer PointLights {
    LightDescriptor pointLights[];
};

layout(set = 1, binding = 1) readonly buffer LightIndexList {
    uint lightIndexList[];
};

layout(set = 2, binding = 0) uniform samplerCube irrMap;
layout(set = 2, binding = 1) uniform samplerCube refMap;
layout(set = 2, binding = 2) uniform sampler2D brdfLut;

// x = offset into lightIndexList, y = number of lights this tile kept.
layout(set = 3, binding = 0, rg32ui) uniform readonly uimage2D lightGrid;

float distributionGGX(const float NdotH, const float roughness) {
    const float a = roughness * roughness;
    const float a2 = a * a;
    const float denom = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * denom * denom);
}

float geometrySchlickGGX(const float NdotV, const float roughness) {
    const float r = roughness + 1.0f;
    const float k = r * r / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float geometrySmith(const float NdotV, const float NdotL, const float roughness) {
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(const float cosTheta, const vec3 F0) {
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

vec3 fresnelSchlickRoughness(const float cosTheta, const vec3 F0, const float roughness) {
    return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

vec3 computeEnvRadiance(
    const vec3 eyeN, const vec3 eyeV, const vec3 kD, const vec3 albedo, const vec3 F, const float roughness) {
    const vec3 worldN = (view.invV * vec4(eyeN, 0.0f)).xyz;
    const vec3 diffuse = texture(irrMap, worldN).rgb * albedo;

    const float NdotV = max(dot(eyeN, eyeV), 0.0f);
    const vec3 worldR = (view.invV * vec4(reflect(-eyeV, eyeN), 0.0f)).xyz;
    const vec3 prefilter = textureLod(refMap, worldR, roughness * kMaxReflectionLod).rgb;
    const vec2 brdf = texture(brdfLut, vec2(NdotV, roughness)).xy;

    return kD * diffuse + prefilter * (F * brdf.x + brdf.y);
}

vec3 evalPointLightRadiance(const LightDescriptor light, out vec3 eyeL) {
    const vec3 eyeLightPos = (view.V * light.position).xyz;
    const vec3 toLight = eyeLightPos - eyePosition;
    const float dist = length(toLight);
    eyeL = toLight / max(dist, 1e-6f);

    const float window = pow(max(0.0f, 1.0f - pow(dist / max(light.params.r, 1e-4f), 4.0f)), 2.0f);
    return light.spectrum.rgb * window / max(dist * dist, 1e-4f);
}

void main() {
    const vec3 eyeN = normalize(eyeNormal);
    const vec3 eyeV = normalize(-eyePosition);
    const float NdotV = max(dot(eyeN, eyeV), 0.0f);

    const vec3 albedo = mat.albedo.rgb;
    const float roughness = clamp(mat.roughness, 0.02f, 1.0f);
    const float metallic = mat.metallic;

    const vec3 F0 = mix(vec3(0.04f), albedo, metallic);
    const vec3 diffuseAlbedo = albedo * (1.0f - metallic);

    const uvec2 tileData = imageLoad(lightGrid, ivec2(gl_FragCoord.xy) / kTileSize).xy;

    if (mat.debugMode == kDebugModeTileHeatmap) {
        fragColor = vec4(heatmap(clamp(float(tileData.y) / kHeatmapRange, 0.0f, 1.0f)), 1.0f);
        return;
    }

    vec3 Lo = vec3(0.0f);
    for (uint i = 0; i < tileData.y; ++i) {
        const LightDescriptor light = pointLights[lightIndexList[tileData.x + i]];

        vec3 eyeL;
        const vec3 Le = evalPointLightRadiance(light, eyeL);
        const float NdotL = max(dot(eyeN, eyeL), 0.0f);
        if (NdotL <= 0.0f) {
            continue;
        }

        const vec3 eyeH = normalize(eyeL + eyeV);
        const float D = distributionGGX(max(dot(eyeN, eyeH), 0.0f), roughness);
        const float G = geometrySmith(NdotV, NdotL, roughness);
        const vec3 F = fresnelSchlick(max(dot(eyeH, eyeV), 0.0f), F0);

        const vec3 specular = D * G * F / max(4.0f * NdotV * NdotL, 1e-4f);
        const vec3 diffuse = (1.0f - F) * diffuseAlbedo / PI;

        Lo += (diffuse + specular) * Le * NdotL;
    }

    const vec3 Fenv = fresnelSchlickRoughness(NdotV, F0, roughness);
    const vec3 kD = (1.0f - Fenv) * (1.0f - metallic);
    Lo += computeEnvRadiance(eyeN, eyeV, kD, albedo, Fenv, roughness);

    fragColor = vec4(Lo, 1.0f);
}
