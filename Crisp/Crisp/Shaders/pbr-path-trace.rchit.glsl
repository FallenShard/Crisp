#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_descriptor_heap : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "Common/math-constants.part.glsl"
#include "Common/warp.part.glsl"
#include "PathTracer/Core/pbr-hit.part.glsl"
#include "PathTracer/Core/pbr-scene.part.glsl"

layout(location = 0) rayPayloadInEXT PbrHitInfo hitInfo;

hitAttributeEXT vec2 barycentric;

const uint kIntegratorSlot = 3u;
const uint kGgxAlbedoLutSlot = 5u;

const uint kMaterialSamplerSlot = 1u;
const uint kGgxAlbedoLutSamplerSlot = 2u;

const uint kInvalidMaterialTextureOffset = 0xFFFFFFFFu;

layout(descriptor_heap, descriptor_stride = 64) uniform texture2D heapTexture2Ds[];
layout(descriptor_heap, descriptor_stride = 64) uniform sampler heapSamplers[];

layout(descriptor_heap, descriptor_stride = 64) uniform IntegratorParams {
    int maxBounces;
    int sampleCount;
    int frameIdx;
    float environmentIntensity;
    uint energyCompensation;
    uint visibilityMask;
} heapIntegrators[];

#define CRISP_GGX_ALBEDO_LUT sampler2D(heapTexture2Ds[kGgxAlbedoLutSlot], heapSamplers[kGgxAlbedoLutSamplerSlot])

#include "PathTracer/BSDFs/pbr-surface.part.glsl"

vec4 sampleMaterialTexture(const uint textureOffset, const uint textureIndex, const vec2 texCoord) {
    const uint heapIndex = nonuniformEXT(textureOffset + textureIndex);
    // Ray tracing stages have no implicit screen-space derivatives; use the base mip until ray differentials land.
    return textureLod(sampler2D(heapTexture2Ds[heapIndex], heapSamplers[kMaterialSamplerSlot]), texCoord, 0.0f);
}

void main() {
    // Not const: the record holds buffer references, which GLSL forbids qualifying.
    PathTracedInstance instance = scene.instances.data[gl_InstanceCustomIndexEXT];
    const uvec3 triangle = instance.triangles.data[gl_PrimitiveID];

    const vec3 baryCoord = vec3(1.0f - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
    const vec3 objectNormal = normalize(
        instance.attributes.data[triangle.x].normal * baryCoord.x +
        instance.attributes.data[triangle.y].normal * baryCoord.y +
        instance.attributes.data[triangle.z].normal * baryCoord.z);
    const vec2 texCoord =
        (instance.attributes.data[triangle.x].texCoord * baryCoord.x +
         instance.attributes.data[triangle.y].texCoord * baryCoord.y +
         instance.attributes.data[triangle.z].texCoord * baryCoord.z);
    const vec4 objectTangent =
        (instance.attributes.data[triangle.x].tangent * baryCoord.x +
         instance.attributes.data[triangle.y].tangent * baryCoord.y +
         instance.attributes.data[triangle.z].tangent * baryCoord.z);

    // Normals transform by the inverse transpose; gl_WorldToObjectEXT already is the inverse, so a row-vector
    // multiply transposes it without inverting anything here.
    const vec3 worldNormal = normalize(objectNormal * mat3(gl_WorldToObjectEXT));

    hitInfo.position = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
    hitInfo.tHit = gl_HitTEXT;

    PbrMaterialParameters material = scene.materials.data[instance.materialIndex];

    // Shade against the side the ray arrived on, so a double-sided surface does not go black from behind.
    const vec3 wiWorld = -gl_WorldRayDirectionEXT;
    vec3 shadingNormal = dot(worldNormal, wiWorld) < 0.0f ? -worldNormal : worldNormal;

    if (instance.materialTextureOffset != kInvalidMaterialTextureOffset) {
        const vec2 scaledTexCoord = texCoord * material.uvScale;
        const vec4 baseColorSample = sampleMaterialTexture(instance.materialTextureOffset, 0u, scaledTexCoord);
        const vec3 ormSample = sampleMaterialTexture(instance.materialTextureOffset, 2u, scaledTexCoord).rgb;
        const vec3 emissionSample = sampleMaterialTexture(instance.materialTextureOffset, 3u, scaledTexCoord).rgb;

        material.baseColor *= baseColorSample.rgb;
        material.baseMetalness *= ormSample.b;
        material.specularRoughness *= ormSample.g;
        hitInfo.emission = emissionSample * max(material.emissionColor, vec3(0.0f)) *
            max(material.emissionLuminance, 0.0f);

        // Match the raster path's tangent-space normal decoding. Geometry without valid UV tangents keeps its
        // interpolated normal rather than allowing NaNs into the path throughput.
        if (!any(isnan(objectTangent.xyz))) {
            vec3 tangent = mat3(gl_ObjectToWorldEXT) * objectTangent.xyz;
            tangent -= shadingNormal * dot(shadingNormal, tangent);
            const float tangentLengthSquared = dot(tangent, tangent);
            if (tangentLengthSquared > 1e-12f) {
                tangent *= inversesqrt(tangentLengthSquared);
                const vec3 bitangent = objectTangent.w * cross(shadingNormal, tangent);
                vec3 mappedNormal =
                    sampleMaterialTexture(instance.materialTextureOffset, 1u, scaledTexCoord).xyz * 2.0f - 1.0f;
                mappedNormal.xy *= material.normalScale;
                shadingNormal = normalize(mat3(tangent, bitangent, shadingNormal) * normalize(mappedNormal));
            }
        }
    } else {
        hitInfo.emission = max(material.emissionColor, vec3(0.0f)) * max(material.emissionLuminance, 0.0f);
    }

    const mat3 frame = createCoordinateFrame(shadingNormal);
    const vec3 wi = transpose(frame) * wiWorld;

    const PbrSurface surface = createPbrSurface(material, heapIntegrators[kIntegratorSlot].energyCompensation);

    vec3 wo;
    float pdf;
    bool sampledSpecular;
    const vec3 weight = samplePbrSurface(surface, hitInfo.unitSample, wi, wo, pdf, sampledSpecular);

    hitInfo.sampleDirection = frame * wo;
    hitInfo.samplePdf = pdf;
    hitInfo.sampleWeight = weight;
    hitInfo.sampleIsSpecular = sampledSpecular ? 1u : 0u;
}
