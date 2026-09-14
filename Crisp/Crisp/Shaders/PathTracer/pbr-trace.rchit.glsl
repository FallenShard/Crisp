#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_descriptor_heap : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../Common/math-constants.part.glsl"
#include "../Common/warp.part.glsl"
#include "Core/heap-slots.part.glsl"
#include "Core/hit-info.part.glsl"
#include "Core/intersection.part.glsl"
#include "Core/pbr-scene.part.glsl"

layout(location = 0) rayPayloadInEXT HitInfo hitInfo;

hitAttributeEXT vec2 barycentric;

layout(descriptor_heap, descriptor_stride = 64) uniform texture2D heapTexture2Ds[];
layout(descriptor_heap, descriptor_stride = 64) uniform sampler heapSamplers[];

layout(descriptor_heap, descriptor_stride = 64) uniform IntegratorParams {
    int maxBounces;
    int sampleCount;
    int frameIdx;
    float environmentIntensity;
    uint energyCompensation;
    uint visibilityMask;
    int environmentWidth;
    int environmentHeight;
} heapIntegrators[];

#define CRISP_GGX_ALBEDO_LUT sampler2D(heapTexture2Ds[kGgxAlbedoLutSlot], heapSamplers[kGgxAlbedoLutSamplerSlot])
#define CRISP_MATERIAL_TEXTURE(heapIndex) sampler2D(heapTexture2Ds[heapIndex], heapSamplers[kMaterialSamplerSlot])

#include "BSDFs/pbr-surface.part.glsl"
#include "Textures/pbr-material-texture.part.glsl"

void main() {
    // Not const: the record holds buffer references, which GLSL forbids qualifying.
    PathTracedInstance instance = scene.instances.data[gl_InstanceCustomIndexEXT];
    const uvec3 triangle = instance.triangles.data[gl_PrimitiveID];

    const vec3 baryCoord = vec3(1.0f - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
    const vec3 objectNormal = interpolateNormal(instance.attributes, triangle, baryCoord);
    const vec2 texCoord = interpolateTexCoord(instance.attributes, triangle, baryCoord);
    const vec4 objectTangent = interpolateTangent(instance.attributes, triangle, baryCoord);

    // Normals transform by the inverse transpose; gl_WorldToObjectEXT already is the inverse, so a row-vector
    // multiply transposes it without inverting anything here.
    const vec3 worldNormal = normalize(objectNormal * mat3(gl_WorldToObjectEXT));

    hitInfo.position = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
    hitInfo.tHit = gl_HitTEXT;
    hitInfo.materialId = instance.materialIndex;
    hitInfo.materialTextureOffset = instance.materialTextureOffset;
    hitInfo.texCoord = texCoord;

    PbrMaterialParameters material = scene.materials.data[instance.materialIndex];

    // Shade against the side the ray arrived on, so a double-sided surface does not go black from behind.
    const vec3 wiWorld = -gl_WorldRayDirectionEXT;
    vec3 shadingNormal = dot(worldNormal, wiWorld) < 0.0f ? -worldNormal : worldNormal;

    hitInfo.Le = applyMaterialTextures(material, instance.materialTextureOffset, texCoord);

    if (instance.materialTextureOffset != kInvalidMaterialTextureOffset) {
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
                    sampleMaterialTexture(
                        instance.materialTextureOffset, kMaterialNormalTexture, texCoord * material.uvScale)
                        .xyz *
                        2.0f -
                    1.0f;
                mappedNormal.xy *= material.normalScale;
                shadingNormal = normalize(mat3(tangent, bitangent, shadingNormal) * normalize(mappedNormal));
            }
        }
    }

    hitInfo.normal = shadingNormal;

    const mat3 frame = createCoordinateFrame(shadingNormal);
    const vec3 wi = transpose(frame) * wiWorld;

    const PbrSurface surface = createPbrSurface(material, heapIntegrators[kIntegratorSlot].energyCompensation);

    vec3 wo;
    float pdf;
    bool sampledSpecular;
    const vec3 weight = samplePbrSurface(surface, hitInfo.bsdfSample, hitInfo.bsdfLobeSample, wi, wo, pdf, sampledSpecular);

    hitInfo.sampleDirection = frame * wo;
    hitInfo.samplePdf = pdf;
    hitInfo.sampleWeight = weight;
    hitInfo.sampleLobeType = sampledSpecular ? kLobeTypeGlossy : kLobeTypeDiffuse;
}
