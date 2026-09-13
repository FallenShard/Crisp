#version 460 core

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_ray_query : require

#include "Common/math-constants.part.glsl"
#include "BSDFs/Microfacet/ggx.part.glsl"
#include "Common/bindless.part.glsl"
#include "Common/view.part.glsl"

const vec3 NdcMin = vec3(-1.0f, -1.0f, 0.0f);
const vec3 NdcMax = vec3(+1.0f, +1.0f, 1.0f);
const uint PbrDrawRayTracedShadows = 1u << 0;

layout(location = 0) in vec3 eyeNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 eyePosition;
layout(location = 3) in vec4 eyeTangent;
layout(location = 4) in vec3 worldPos;

layout(location = 0) out vec4 fragColor;

struct LightDescriptor {
    mat4 V;
    mat4 P;
    mat4 VP;
    vec4 position;
    vec4 direction;
    vec4 spectrum;
    vec4 params;
};

// View-specific parameters.
layout(set = 1, binding = 0) uniform View {
    ViewParameters view;
};
layout(set = 1, binding = 1) uniform CascadedLight {
    LightDescriptor cascadedLight[4];
};
layout(set = 1, binding = 2) uniform samplerCube diffuseIrradianceMap;
layout(set = 1, binding = 3) uniform samplerCube specularReflectanceMap;
layout(set = 1, binding = 4) uniform sampler2D cascadedShadowMaps[4];
layout(set = 1, binding = 5) uniform sampler2D brdfLut;
layout(set = 1, binding = 6) uniform accelerationStructureEXT shadowSceneBvh;

// Material-specific parameters. Must match PbrMaterialParams in Materials/PbrMaterial.hpp.
#include "Common/openpbr-surface.part.glsl"

// The OpenPBR half is one nested block shared with the rasterizer, the other tracer and BsdfParameters;
// everything after it is a Crisp renderer extension. Must match PbrMaterialParams in Materials/PbrMaterial.hpp.
struct PbrMaterialParameters {
    OpenPbrSurfaceParams surface;

    vec2 uvScale;
    float normalScale;
    float aoStrength;

    uint samplerIndex;
    uint baseColorTex;
    uint normalTex;
    uint ormTex;

    uint emissionTex;
    float geometryOpacity;
    float alphaCutoff;
    uint flags;
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer PbrMaterialTable {
    PbrMaterialParameters materials[];
};

layout(push_constant) uniform DrawParameters {
    PbrMaterialTable materialTable;
    uint materialIndex;
    uint flags;
}
drawParameters;

// The indices come from one material selected by push constants, so they are dynamically uniform and skip
// nonuniformEXT.
vec4 sampleMaterial(const PbrMaterialParameters material, const uint textureIndex, const vec2 uv) {
    return texture(sampler2D(gTextures2D[textureIndex], gSamplers[material.samplerIndex]), uv);
}

vec3 evalDirectionalLightRadiance(out vec3 eyeL) {
    eyeL = normalize((view.V * cascadedLight[0].direction).xyz);
    return cascadedLight[0].spectrum.rgb;
}

// ----- Cascaded Shadow Mapping
float sampleCascadeShadow(const int cascadeIndex, const vec3 worldPos, const vec3 worldNormal) {
    const vec3 worldLightDirection = normalize(cascadedLight[cascadeIndex].direction.xyz);
    const float NdotL = clamp(dot(worldNormal, worldLightDirection), 0.0f, 1.0f);

    const float sinTheta = sqrt(max(1.0f - NdotL * NdotL, 0.0f));
    const float normalBiasInTexels = 1.5f;
    const float normalBias = normalBiasInTexels * cascadedLight[cascadeIndex].params.w * sinTheta;
    const vec3 biasedWorldPos = worldPos + worldNormal * normalBias;

    const vec4 lightSpacePos = cascadedLight[cascadeIndex].VP * vec4(biasedWorldPos, 1.0f);
    const vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;

    if (any(lessThan(ndcPos, NdcMin)) || any(greaterThan(ndcPos, NdcMax))) {
        return 1.0f;
    }

    const vec2 texCoord = ndcPos.xy * 0.5f + 0.5f;

    const ivec2 size = textureSize(cascadedShadowMaps[nonuniformEXT(cascadeIndex)], 0).xy;
    const vec2 texelSize = vec2(1) / size;

    const float biasInTexels = mix(1.25f, 0.35f, NdotL);
    const float worldBias = biasInTexels * cascadedLight[cascadeIndex].params.w;
    const float depthBias = worldBias * abs(cascadedLight[cascadeIndex].P[2][2]);

    const int pcfRadius = 2;
    const float numSamples = (2 * pcfRadius + 1) * (2 * pcfRadius + 1);

    float amount = 0.0f;
    for (int i = -pcfRadius; i <= pcfRadius; i++) {
        for (int j = -pcfRadius; j <= pcfRadius; j++) {
            const vec2 tc = texCoord + vec2(i, j) * texelSize;
            const float shadowMapDepth = texture(cascadedShadowMaps[nonuniformEXT(cascadeIndex)], tc).r;
            amount += shadowMapDepth < ndcPos.z - depthBias ? 0.0f : 1.0f;
        }
    }

    return amount / numSamples;
}

int selectCascade(const float viewDepth) {
    for (int i = 0; i < 4; ++i) {
        if (viewDepth <= cascadedLight[i].params.y) {
            return i;
        }
    }
    return -1;
}

float evalCascadedShadow(const vec3 worldPos, const float viewDepth, const vec3 worldNormal) {
    const int cascadeIndex = selectCascade(viewDepth);
    if (cascadeIndex < 0) {
        return 1.0f;
    }

    const float currentShadow = sampleCascadeShadow(cascadeIndex, worldPos, worldNormal);
    if (cascadeIndex == 3 || viewDepth <= cascadedLight[cascadeIndex].params.z) {
        return currentShadow;
    }

    const float blend = smoothstep(
        cascadedLight[cascadeIndex].params.z, cascadedLight[cascadeIndex].params.y, viewDepth);
    return mix(currentShadow, sampleCascadeShadow(cascadeIndex + 1, worldPos, worldNormal), blend);
}

float evalRayTracedShadow(const vec3 worldPos, const vec3 worldNormal) {
    const vec3 worldLightDirection = normalize(cascadedLight[0].direction.xyz);
    if (dot(worldNormal, worldLightDirection) <= 0.0f) {
        return 1.0f;
    }

    
    const float originOffset = 0.0005f;
    rayQueryEXT query;
    rayQueryInitializeEXT(
        query,
        shadowSceneBvh,
        gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT,
        0xff,
        worldPos + worldNormal * originOffset,
        0.0f,
        worldLightDirection,
        1000000.0f);
    rayQueryProceedEXT(query);

    return rayQueryGetIntersectionTypeEXT(query, true) == gl_RayQueryCommittedIntersectionNoneEXT ? 1.0f : 0.0f;
}

vec3 getCascadeDebugColor(const float viewDepth) {
    const vec3 cascadeColors[4] = vec3[4](
        vec3(1.0f, 0.15f, 0.12f),
        vec3(0.15f, 1.0f, 0.20f),
        vec3(0.15f, 0.35f, 1.0f),
        vec3(1.0f, 0.25f, 0.90f));

    const int cascadeIndex = selectCascade(viewDepth);
    if (cascadeIndex < 0) {
        return vec3(0.25f);
    }
    if (cascadeIndex == 3 || viewDepth <= cascadedLight[cascadeIndex].params.z) {
        return cascadeColors[cascadeIndex];
    }

    const float blend = smoothstep(
        cascadedLight[cascadeIndex].params.z, cascadedLight[cascadeIndex].params.y, viewDepth);
    return mix(cascadeColors[cascadeIndex], cascadeColors[cascadeIndex + 1], blend);
}

vec3 computeEnvRadiance(vec3 eyeN, vec3 eyeV, vec3 kD, vec3 albedo, vec3 F0, float roughness, float ao) {
    const vec3 worldN = (view.invV * vec4(eyeN, 0.0f)).rgb;
    const vec3 irradiance = texture(diffuseIrradianceMap, worldN).rgb;
    const vec3 diffuse = irradiance * albedo;

    const float NdotV = max(dot(eyeN, eyeV), 0.0f);
    const vec3 eyeR = reflect(-eyeV, eyeN);
    const vec3 worldR = (view.invV * vec4(eyeR, 0.0f)).rgb;

    const float maxReflectionLod = float(max(textureQueryLevels(specularReflectanceMap) - 1, 0));
    const vec3 prefilter = textureLod(specularReflectanceMap, worldR, roughness * maxReflectionLod).rgb;
    const vec2 brdf = texture(brdfLut, vec2(NdotV, roughness)).xy;
    const vec3 specularAlbedo = F0 * brdf.x + brdf.y;

    return (1.0f - specularAlbedo) * kD * diffuse * ao + prefilter * specularAlbedo;
}

float computeDielectricF0(const float ior, const float weight) {
    const float eta = max(ior, 0.001f);
    const float unweightedF0 = pow((1.0f - eta) / (1.0f + eta), 2.0f);
    return clamp(max(weight, 0.0f) * unweightedF0, 0.0f, 0.9999f);
}

vec3 evaluateOrenNayarDiffuse(
    const vec3 color,
    const float roughness,
    const vec3 eyeN,
    const vec3 eyeL,
    const vec3 eyeV,
    const float NdotL,
    const float NdotV) {
    const float sigma2 = roughness * roughness;
    const float A = 1.0f - 0.5f * sigma2 / (sigma2 + 0.33f);
    const float B = 0.45f * sigma2 / (sigma2 + 0.09f);
    const float s = dot(eyeL, eyeV) - NdotL * NdotV;
    const float t = s > 0.0f ? max(NdotL, NdotV) : 1.0f;
    return color * (A + B * s / max(t, 0.001f)) / PI;
}

vec3 decodeNormal(const PbrMaterialParameters material, in vec2 uv) {
    vec3 normal = normalize(eyeNormal);
    // Have to check this because without UVs, computed tangents will be NaN.
    if (any(isnan(eyeTangent.xyz))) {
        return normal;
    }

    vec3 tangent = normalize(eyeTangent.xyz - normal * dot(normal, eyeTangent.xyz));
    vec3 bitangent = eyeTangent.w * cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    vec3 n = sampleMaterial(material, material.normalTex, uv).xyz * 2.0f - 1.0f;
    n.xy *= material.normalScale;
    return normalize(TBN * normalize(n));
}

void main() {
    const PbrMaterialParameters material = drawParameters.materialTable.materials[drawParameters.materialIndex];
    const vec2 uvCoord = inTexCoord * material.uvScale;

    // Basic shading geometry.
    const vec3 geometricEyeN = normalize(eyeNormal);
    const vec3 eyeN = decodeNormal(material, uvCoord);
    const vec3 eyeV = normalize(-eyePosition);
    const float NdotV = max(dot(eyeN, eyeV), 0.0f);

    // Primary light source radiance.
    vec3 eyeL; // light direction, out param
    const vec3 Le = evalDirectionalLightRadiance(eyeL);
    const float NdotL = max(dot(eyeN, eyeL), 0.0f);

    // Material properties.
    const vec4 baseColorSample = sampleMaterial(material, material.baseColorTex, uvCoord);
    const float opacity = baseColorSample.a * material.geometryOpacity;
    if ((material.flags & 1u) != 0u && opacity < material.alphaCutoff) {
        discard;
    }
    const vec3 baseColor = baseColorSample.rgb * material.surface.baseColor;
    const vec3 orm = sampleMaterial(material, material.ormTex, uvCoord).rgb;
    const float specularRoughness = clamp(orm.g * material.surface.specularRoughness, 0.001f, 1.0f);
    const float alpha = specularRoughness * specularRoughness;
    const float baseMetalness = clamp(orm.b * material.surface.baseMetalness, 0.0f, 1.0f);
    const float ao = mix(1.0f, orm.r, clamp(material.aoStrength, 0.0f, 1.0f));
    const vec3 emission = sampleMaterial(material, material.emissionTex, uvCoord).rgb * material.surface.emissionColor *
        max(material.surface.emissionLuminance, 0.0f);

    // Environment BRDF.
    const vec3 dielectricF0 = computeDielectricF0(material.surface.specularIor, material.surface.specularWeight) *
        clamp(material.surface.specularColor, vec3(0.0f), vec3(1.0f));
    const vec3 F0 = mix(dielectricF0, baseColor, baseMetalness);
    const vec3 envKd = vec3(1.0f - baseMetalness);

    // Direct-light BRDF.
    const vec3 eyeH = normalize(eyeL + eyeV);
    const float NdotH = max(dot(eyeN, eyeH), 0.0f);
    const float VdotH = max(dot(eyeV, eyeH), 0.0f);
    const vec3 directF = fresnelSchlick(VdotH, F0);
    const vec3 directKd = (1.0f - directF) * (1.0f - baseMetalness);
    const vec3 directDiffuse = directKd * evaluateOrenNayarDiffuse(
        baseColor,
        clamp(material.surface.baseDiffuseRoughness, 0.0f, 1.0f),
        eyeN,
        eyeL,
        eyeV,
        NdotL,
        NdotV);
    const float D = distributionGgx(NdotH, alpha);
    const float G = geometrySmith(NdotV, NdotL, specularRoughness);
    const vec3 directSpecular = D * G * directF / max(4.0f * NdotV * NdotL, 0.001);

    const vec3 geometricWorldN = normalize((view.invV * vec4(geometricEyeN, 0.0f)).xyz);
    float shadowCoeff = 1.0f;
    if ((drawParameters.flags & PbrDrawRayTracedShadows) != 0u) {
        shadowCoeff = evalRayTracedShadow(worldPos, geometricWorldN);
    } else {
        shadowCoeff = evalCascadedShadow(worldPos, -eyePosition.z, geometricWorldN);
    }

    const vec3 directRadiance = (directDiffuse + directSpecular) * Le * NdotL;
    const vec3 environmentRadiance = computeEnvRadiance(eyeN, eyeV, envKd, baseColor, F0, specularRoughness, ao);

    vec3 color = clamp(material.surface.baseWeight, 0.0f, 1.0f) * (environmentRadiance + shadowCoeff * directRadiance) + emission;
    if (cascadedLight[0].position.w > 0.5f) {
        color = mix(color, getCascadeDebugColor(-eyePosition.z), 0.45f);
    }
    fragColor = vec4(color, 1.0f);
}
