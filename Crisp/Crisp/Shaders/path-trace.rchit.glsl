#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "PathTracer/Core/types.part.glsl"
#include "Common/math-constants.part.glsl"
#include "Common/rng.part.glsl"
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
    // Grab the ID of the object that we just hit.
    const uint objId = gl_InstanceCustomIndexEXT;
    const InstanceProperties props = scene.instances.data[objId];

    // Formulate the triangle at the hit.
    const uvec3 hitTriangle = scene.triangles.data[props.indexOffset + gl_PrimitiveID];

    const vec3 baryCoord = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
    const vec3 normal = interpolateNormal(hitTriangle, baryCoord);
    const vec3 position = interpolatePosition(hitTriangle, baryCoord);

    // Record the hit info for the calling shader.
    hitInfo.position = position;
    hitInfo.tHit = gl_HitTEXT;
    hitInfo.normal = normal;

    // Determine sampled BRDF and the new path direction.

    const mat3 worldTransform = createCoordinateFrame(normal);

    bsdf.normal = toLocal(normal, worldTransform);
    bsdf.wi = toLocal(-gl_WorldRayDirectionEXT, worldTransform);
    bsdf.materialId = props.materialId;
    bsdf.operation = kBrdfOperationSample;

    const float r1 = rndFloat(hitInfo.rngSeed);
    const float r2 = rndFloat(hitInfo.rngSeed);
    bsdf.unitSample = vec2(r1, r2);

    const int brdfType = scene.materials.data[props.materialId].type;
    executeCallableEXT(brdfType, /*location(bsdf)=*/0);
    hitInfo.sampleDirection = toWorld(bsdf.wo, worldTransform);
    hitInfo.samplePdf = bsdf.pdf;
    hitInfo.sampleWeight = bsdf.pdf > 0.0f ? bsdf.f / bsdf.pdf : vec3(0.0f);
    hitInfo.sampleLobeType = bsdf.lobeType;
    hitInfo.materialId = props.materialId;

    // Account for any lights hit.
    hitInfo.Le = vec3(0.0f);
    hitInfo.lightId = -1;
    if (props.lightId != -1) {
        hitInfo.Le = evalAreaLight(position, normal, scene.lights.data[props.lightId].radiance);
        hitInfo.lightId = props.lightId;
    }
}
