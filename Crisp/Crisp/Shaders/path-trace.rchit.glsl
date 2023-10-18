#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "Parts/path-tracer-payload.part.glsl"
#include "Parts/math-constants.part.glsl"
#include "Parts/rng.part.glsl"
#include "Parts/warp.part.glsl"

layout(location = 0) rayPayloadInEXT HitInfo hitInfo;
layout(location = 1) callableDataEXT BsdfSample bsdfSample;

const uint kLambertianShaderCallable = 0;
const uint kDielectricShaderCallable = 1;
const uint kMirrorShaderCallable = 2;

hitAttributeEXT vec2 barycentric;

layout(set = 1, binding = 0, scalar) buffer Vertices
{
    float v[];
} vertices[6];
layout(set = 1, binding = 1, scalar) buffer Indices
{
    uint i[];
} indices[6];

vec3 evalAreaLight(vec3 p, vec3 n)
{
    const vec3 ref = gl_WorldRayOriginEXT;
    const vec3 wi = p - ref;
    const float cosTheta = dot(n, normalize(-wi));
    return cosTheta <= 0.0f ? vec3(0.0f) : vec3(15.0f);
}

void coordinateSystem(in vec3 v1, out vec3 v2, out vec3 v3)
{
    if (abs(v1.x) > abs(v1.y))
    {
        float invLen = 1.0f / sqrt(v1.x * v1.x + v1.z * v1.z);
        v3 = vec3(v1.z * invLen, 0.0f, -v1.x * invLen);
    }
    else
    {
        float invLen = 1.0f / sqrt(v1.y * v1.y + v1.z * v1.z);
        v3 = vec3(0.0f, v1.z * invLen, -v1.y * invLen);
    }
    v2 = cross(v3, v1);
}

const int kVertexComponentCount = 6;
const int kPositionComponentOffset = 0;
const int kNormalComponentOffset = 3;

vec3 getNormal(const uint objectId, const ivec3 ind, const vec3 bary)
{
    mat3 normals = mat3(
        vec3(
            vertices[objectId].v[kVertexComponentCount * ind.x + kNormalComponentOffset],
            vertices[objectId].v[kVertexComponentCount * ind.x + kNormalComponentOffset + 1],
            vertices[objectId].v[kVertexComponentCount * ind.x + kNormalComponentOffset + 2]),
        vec3(
            vertices[objectId].v[kVertexComponentCount * ind.y + kNormalComponentOffset],
            vertices[objectId].v[kVertexComponentCount * ind.y + kNormalComponentOffset + 1],
            vertices[objectId].v[kVertexComponentCount * ind.y + kNormalComponentOffset + 2]),
        vec3(
            vertices[objectId].v[kVertexComponentCount * ind.z + kNormalComponentOffset],
            vertices[objectId].v[kVertexComponentCount * ind.z + kNormalComponentOffset + 1],
            vertices[objectId].v[kVertexComponentCount * ind.z + kNormalComponentOffset + 2])
    );
    return normalize(normals * bary);
}

vec3 getPosition(uint objectId, ivec3 ind, vec3 bary)
{
    mat3 positions = mat3(
        vec3(
            vertices[objectId].v[kVertexComponentCount * ind.x + kPositionComponentOffset],
            vertices[objectId].v[kVertexComponentCount * ind.x + kPositionComponentOffset + 1],
            vertices[objectId].v[kVertexComponentCount * ind.x + kPositionComponentOffset + 2]),
        vec3(
            vertices[objectId].v[kVertexComponentCount * ind.y + kPositionComponentOffset],
            vertices[objectId].v[kVertexComponentCount * ind.y + kPositionComponentOffset + 1],
            vertices[objectId].v[kVertexComponentCount * ind.y + kPositionComponentOffset + 2]),
        vec3(
            vertices[objectId].v[kVertexComponentCount * ind.z + kPositionComponentOffset],
            vertices[objectId].v[kVertexComponentCount * ind.z + kPositionComponentOffset + 1],
            vertices[objectId].v[kVertexComponentCount * ind.z + kPositionComponentOffset + 2])
    );
    return positions * bary;
}

vec3 getAlbedo(const uint objId)
{
    if (objId == 0)
        return vec3(0.725, 0.71, 0.68); // Top, bottom, back wall.
    if (objId == 1)
        return vec3(0.630, 0.065, 0.05); // Left wall.
    if (objId == 2)
        return vec3(0.161, 0.133, 0.427); // Right wall.
    if (objId == 3)
        return vec3(0.5f); // Area light.
    
    return vec3(1.0f);
}

void main()
{
    // Grab the ID of the object that we just hit.
    const uint objId = gl_InstanceCustomIndexEXT;

    // Formulate the triangle at the hit.
    const ivec3 hitTriangle = ivec3(
        indices[objId].i[3 * gl_PrimitiveID + 0],
        indices[objId].i[3 * gl_PrimitiveID + 1],
        indices[objId].i[3 * gl_PrimitiveID + 2]);

    const vec3 baryCoord = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
    const vec3 normal   = getNormal(objId, hitTriangle, baryCoord);
    const vec3 position = getPosition(objId, hitTriangle, baryCoord);

    // Record the hit info for the calling shader.
    hitInfo.position = position;
    hitInfo.tHit = gl_HitTEXT;

    // Determine sampled BRDF and the new path direction.
    if (objId <= 3)
    {
        const float r1 = rndFloat(hitInfo.rngSeed);
        const float r2 = rndFloat(hitInfo.rngSeed);

        bsdfSample.unitSample = vec2(r1, r2);
        bsdfSample.normal = normal;
        executeCallableEXT(kLambertianShaderCallable, 1);

        hitInfo.sampleDirection = bsdfSample.sampleDirection;
        hitInfo.samplePdf = bsdfSample.samplePdf;
        hitInfo.bsdfEval = getAlbedo(objId);
    }
    else if (objId == 4)
    {
        const float r1 = rndFloat(hitInfo.rngSeed);

        bsdfSample.unitSample = vec2(r1, r1);
        bsdfSample.normal = normal;
        bsdfSample.wi = -gl_WorldRayDirectionEXT;
        executeCallableEXT(kDielectricShaderCallable, 1);

        hitInfo.sampleDirection = bsdfSample.sampleDirection;
        hitInfo.samplePdf = bsdfSample.samplePdf;
        hitInfo.bsdfEval = bsdfSample.eval;
    }
    else if (objId == 5)
    {
        bsdfSample.normal = normal;
        bsdfSample.wi = -gl_WorldRayDirectionEXT;
        executeCallableEXT(kMirrorShaderCallable, 1);

        hitInfo.sampleDirection = bsdfSample.sampleDirection;
        hitInfo.samplePdf = bsdfSample.samplePdf;
        hitInfo.bsdfEval = bsdfSample.eval;
    }

    // Account for any lights hit.    
    hitInfo.Le = vec3(0.0f);
    if (objId == 3)
    {
        hitInfo.Le = evalAreaLight(position, normal);// eval light
    }
}
