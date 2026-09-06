#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "PathTracer/Core/types.part.glsl"
#include "Common/math-constants.part.glsl"
#include "Common/warp.part.glsl"

layout(location = 0) rayPayloadInEXT HitInfo hitInfo;
layout(location = 0) callableDataEXT BrdfSample bsdf;

hitAttributeEXT vec2 barycentric;

#include "PathTracer/Core/scene.part.glsl"
#include "PathTracer/Core/intersection.part.glsl"
#include "PathTracer/Lights/area-light.part.glsl"

vec3 toLocal(const vec3 dir, const mat3 coordinateFrame) {
    return transpose(coordinateFrame) * dir;
}

vec3 toWorld(const vec3 dir, const mat3 coordinateFrame) {
    return coordinateFrame * dir;
}

void main() {
    InstanceProperties instance = scene.instances.data[gl_InstanceCustomIndexEXT];

    // Formulate the triangle at the hit.
    const uvec3 hitTriangle = instance.triangles.data[gl_PrimitiveID];

    const vec3 baryCoord = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
    const vec3 normal = interpolateNormal(instance.normals, hitTriangle, baryCoord);
    const vec3 position = interpolatePosition(instance.positions, hitTriangle, baryCoord);
    const vec2 texCoord = interpolateTexCoord(instance.texCoords, hitTriangle, baryCoord);

    // Record the hit info for the calling shader.
    hitInfo.position = position;
    hitInfo.tHit = gl_HitTEXT;
    hitInfo.normal = normal;
    hitInfo.texCoord = texCoord;

    // Determine sampled BRDF and the new path direction.

    const mat3 worldTransform = createCoordinateFrame(normal);

    bsdf.normal = toLocal(normal, worldTransform);
    bsdf.wi = toLocal(-gl_WorldRayDirectionEXT, worldTransform);
    bsdf.materialId = instance.materialId;
    bsdf.operation = kBrdfOperationSample;
    bsdf.texCoord = texCoord;

    bsdf.unitSample = hitInfo.bsdfSample;
    bsdf.lobeSample = hitInfo.bsdfLobeSample;

    const int brdfType = scene.materials.data[instance.materialId].type;
    executeCallableEXT(brdfType, /*location(bsdf)=*/0);
    hitInfo.sampleDirection = toWorld(bsdf.wo, worldTransform);
    hitInfo.samplePdf = bsdf.pdf;
    hitInfo.sampleWeight = bsdf.pdf > 0.0f ? bsdf.f / bsdf.pdf : vec3(0.0f);
    hitInfo.sampleLobeType = bsdf.lobeType;
    hitInfo.materialId = instance.materialId;

    // Account for any lights hit.
    hitInfo.Le = vec3(0.0f);
    hitInfo.lightId = -1;
    if (instance.lightId != -1) {
        hitInfo.Le = evalAreaLight(position, normal, scene.lights.data[instance.lightId].emission);
        hitInfo.lightId = instance.lightId;
    }
}
