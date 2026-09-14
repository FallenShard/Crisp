#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "Core/heap-slots.part.glsl"
#include "Core/types.part.glsl"
#include "../Common/math-constants.part.glsl"
#include "../Common/warp.part.glsl"

layout(location = 0) rayPayloadInEXT HitInfo hitInfo;

hitAttributeEXT vec2 barycentric;

// The OpenPBR lobe reads the GGX directional-albedo table unconditionally, even with compensation
// off; material-texture.part.glsl declares the heap arrays this expands to.
#define CRISP_GGX_ALBEDO_LUT sampler2D(heapTexture2Ds[kGgxAlbedoLutSlot], heapSamplers[kGgxAlbedoLutSamplerSlot])

#include "Core/scene.part.glsl"
#include "Core/intersection.part.glsl"
#include "BSDFs/bsdf-sample.part.glsl"
#include "Lights/area-light.part.glsl"

vec3 toLocal(const vec3 dir, const mat3 coordinateFrame) {
    return transpose(coordinateFrame) * dir;
}

vec3 toWorld(const vec3 dir, const mat3 coordinateFrame) {
    return coordinateFrame * dir;
}

void main() {
    PathTracedInstance instance = scene.instances.data[gl_InstanceCustomIndexEXT];

    // Formulate the triangle at the hit.
    const uvec3 hitTriangle = instance.triangles.data[gl_PrimitiveID];

    const vec3 baryCoord = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
    const vec3 normal = interpolateNormal(instance.attributes, hitTriangle, baryCoord);
    const vec3 position = interpolatePosition(instance.positions, hitTriangle, baryCoord);
    const vec2 texCoord = interpolateTexCoord(instance.attributes, hitTriangle, baryCoord);

    // Record the hit info for the calling shader.
    hitInfo.position = position;
    hitInfo.tHit = gl_HitTEXT;
    hitInfo.normal = normal;
    hitInfo.texCoord = texCoord;

    // Determine the sampled BSDF and new path direction.

    const mat3 worldTransform = createCoordinateFrame(normal);

    BsdfSample bsdf;
    bsdf.wi = toLocal(-gl_WorldRayDirectionEXT, worldTransform);
    bsdf.materialId = instance.materialIndex;
    bsdf.texCoord = texCoord;

    bsdf.unitSample = hitInfo.bsdfSample;
    bsdf.lobeSample = hitInfo.bsdfLobeSample;

    sampleBsdf(bsdf);
    hitInfo.sampleDirection = toWorld(bsdf.wo, worldTransform);
    hitInfo.samplePdf = bsdf.pdf;
    hitInfo.sampleWeight = bsdf.pdf > 0.0f ? bsdf.f / bsdf.pdf : vec3(0.0f);
    hitInfo.sampleLobeType = bsdf.lobeType;
    hitInfo.materialId = instance.materialIndex;

    // Account for any lights hit.
    hitInfo.Le = vec3(0.0f);
    hitInfo.lightId = -1;
    if (instance.lightId != -1) {
        hitInfo.Le = evalAreaLight(position, normal, scene.lights.data[instance.lightId].emission);
        hitInfo.lightId = instance.lightId;
    }
}
