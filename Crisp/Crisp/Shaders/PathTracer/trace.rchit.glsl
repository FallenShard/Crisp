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
#include "Core/types.part.glsl"

layout(location = 0) rayPayloadInEXT HitInfo hitInfo;

hitAttributeEXT vec2 barycentric;

// material-texture.part.glsl declares the heap arrays these expand to. The OpenPBR lobe reads the GGX
// directional-albedo table unconditionally, even with compensation off.
#define CRISP_GGX_ALBEDO_LUT sampler2D(heapTexture2Ds[kGgxAlbedoLutSlot], heapSamplers[kGgxAlbedoLutSamplerSlot])
#define CRISP_MATERIAL_TEXTURE(heapIndex) sampler2D(heapTexture2Ds[heapIndex], heapSamplers[kMaterialSamplerSlot])

#include "Core/scene-addresses.part.glsl"
#include "Core/intersection.part.glsl"
#include "BSDFs/bsdf-sample.part.glsl"
#include "Textures/pbr-material-texture.part.glsl"
#include "Lights/area-light.part.glsl"

void main() {
    PathTracedInstance instance = scene.instances.data[gl_InstanceCustomIndexEXT];
    const uvec3 triangle = instance.triangles.data[gl_PrimitiveID];

    const vec3 baryCoord = vec3(1.0f - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
    const vec2 texCoord = interpolateTexCoord(instance.attributes, triangle, baryCoord);
    const vec4 objectTangent = interpolateTangent(instance.attributes, triangle, baryCoord);

    const vec3 normal = normalize(interpolateNormal(instance.attributes, triangle, baryCoord) * mat3(gl_WorldToObjectEXT));
    const vec3 position =
        gl_ObjectToWorldEXT * vec4(interpolatePosition(instance.positions, triangle, baryCoord), 1.0f);

    hitInfo.position = position;
    hitInfo.tHit = gl_HitTEXT;
    hitInfo.materialId = instance.materialIndex;
    hitInfo.materialTextureOffset = instance.materialTextureOffset;
    hitInfo.texCoord = texCoord;

    PbrMaterialParameters material = scene.materials.data[instance.materialIndex];

    const vec3 wiWorld = -gl_WorldRayDirectionEXT;

    // Shade against the side the ray arrived on, so a single-sided surface does not go black from behind. Opt-in
    // per instance: a transmissive lobe needs the geometric side to tell inside from outside.
    const bool twoSided = (instance.flags & kInstanceTwoSidedShading) != 0u;
    vec3 shadingNormal = twoSided && dot(normal, wiWorld) < 0.0f ? -normal : normal;

    // Folds the material's maps into its parameters. A no-op for an instance with no texture block, which is how
    // a scene that reaches its reflectance through material.reflectanceTexture instead is left untouched.
    hitInfo.Le = applyMaterialTextures(material, instance.materialTextureOffset, texCoord);

    if (instance.materialTextureOffset != kInvalidMaterialTextureOffset) {
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

    BsdfSample bsdf;
    bsdf.unitSample = hitInfo.bsdfSample;
    bsdf.lobeSample = hitInfo.bsdfLobeSample;
    bsdf.texCoord = texCoord;
    bsdf.wi = transpose(frame) * wiWorld;

    sampleBsdf(material, bsdf);

    hitInfo.sampleDirection = frame * bsdf.wo;
    hitInfo.samplePdf = bsdf.pdf;
    hitInfo.sampleWeight = bsdf.weight;
    hitInfo.sampleLobeType = bsdf.lobeType;

    hitInfo.lightId = -1;
    if (instance.lightId != -1) {
        hitInfo.Le = evalAreaLight(position, normal, scene.lights.data[instance.lightId].emission);
        hitInfo.lightId = instance.lightId;
    }
}
