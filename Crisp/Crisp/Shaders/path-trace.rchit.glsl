#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "Common/path-trace-payload.part.glsl"
#include "Common/math-constants.part.glsl"
#include "Common/rng.part.glsl"
#include "Common/warp.part.glsl"

layout(location = 0) rayPayloadInEXT HitInfo hitInfo;
layout(location = 1) callableDataEXT BrdfSample bsdf;

const uint kLambertianShaderCallable = 0;
const uint kDielectricShaderCallable = 1;
const uint kMirrorShaderCallable = 2;
const uint kMicrofacetShaderCallable = 3;

hitAttributeEXT vec2 barycentric;

layout(set = 1, binding = 0, scalar) buffer Vertices {
    vec3 data[];
} vertices;

layout(set = 1, binding = 1, scalar) buffer Indices {
    uvec3 data[];
} triangles;

layout(set = 1, binding = 6, scalar) buffer Normals {
    vec3 data[];
} normals;

layout(set = 1, binding = 2, scalar) buffer InstanceProps {
    InstanceProperties instanceProps[];
};

layout(set = 1, binding = 3) buffer BrdfParams {
    BrdfParameters brdfParams[];
};

layout(set = 1, binding = 4, std430) buffer Lights {
    LightParameters lights[];
};

vec3 evalAreaLight(vec3 p, vec3 n, vec3 radiance) {
    const vec3 ref = gl_WorldRayOriginEXT;
    const vec3 wi = p - ref;
    const float cosTheta = dot(n, normalize(-wi));
    return cosTheta <= 0.0f ? vec3(0.0f) : radiance;
}

#include "Common/path-trace-vertex-pull.part.glsl"

vec3 toLocal(const vec3 dir, const mat3 coordinateFrame) {
    return transpose(coordinateFrame) * dir;
}

vec3 toWorld(const vec3 dir, const mat3 coordinateFrame) {
    return coordinateFrame * dir;
}

void main() {
    // Grab the ID of the object that we just hit.
    const uint objId = gl_InstanceCustomIndexEXT;
    const InstanceProperties props = instanceProps[objId];

    // Formulate the triangle at the hit.
    const uvec3 hitTriangle = triangles.data[props.indexOffset + gl_PrimitiveID];

    const vec3 baryCoord = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
    const vec3 normal   = interpolateNormal(hitTriangle, baryCoord);
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

    const float r1 = rndFloat(hitInfo.rngSeed);
    const float r2 = rndFloat(hitInfo.rngSeed);
    bsdf.unitSample = vec2(r1, r2);

    const int brdfType = brdfParams[props.materialId].type;
    executeCallableEXT(brdfType, 1);
    hitInfo.sampleDirection = toWorld(bsdf.wo, worldTransform);
    hitInfo.samplePdf = bsdf.pdf;
    hitInfo.bsdfEval = bsdf.f;
    hitInfo.sampleLobeType = bsdf.lobeType;

    // Account for any lights hit.    
    hitInfo.Le = vec3(0.0f);
    hitInfo.lightId = -1;
    if (props.lightId != -1) {
        hitInfo.Le = evalAreaLight(position, normal, lights[props.lightId].radiance);
        hitInfo.lightId = props.lightId;
    }
}
