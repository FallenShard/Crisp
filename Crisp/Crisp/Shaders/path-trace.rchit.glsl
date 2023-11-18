#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "Parts/path-tracer-payload.part.glsl"
#include "Parts/math-constants.part.glsl"
#include "Parts/rng.part.glsl"
#include "Parts/warp.part.glsl"

layout(location = 0) rayPayloadInEXT HitInfo hitInfo;
layout(location = 1) callableDataEXT BsdfSample bsdf;

const uint kLambertianShaderCallable = 0;
const uint kDielectricShaderCallable = 1;
const uint kMirrorShaderCallable = 2;
const uint kMicrofacetShaderCallable = 3;

hitAttributeEXT vec2 barycentric;

layout(set = 1, binding = 0, scalar) buffer Vertices
{
    float v[];
} vertices[6];
layout(set = 1, binding = 1, scalar) buffer Indices
{
    uint i[];
} indices[6];

layout(set = 1, binding = 2, scalar) buffer MaterialIds
{
    uint materialIds[];
};

layout(set = 1, binding = 3) buffer BrdfParams
{
    BrdfParameters brdfParams[];
};

vec3 evalAreaLight(vec3 p, vec3 n)
{
    const vec3 ref = gl_WorldRayOriginEXT;
    const vec3 wi = p - ref;
    const float cosTheta = dot(n, normalize(-wi));
    return cosTheta <= 0.0f ? vec3(0.0f) : vec3(15.0f);
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

vec3 toLocal(const vec3 dir, const mat3 coordinateFrame)
{
    return transpose(coordinateFrame) * dir;
}

vec3 toWorld(const vec3 dir, const mat3 coordinateFrame)
{
    return coordinateFrame * dir;
}

float miWeight(float pdf1, float pdf2) {
    pdf1 *= pdf1;
    pdf2 *= pdf2;
    return pdf1 / (pdf1 + pdf2);
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

    const uint materialId = materialIds[objId];
    const int brdfType = brdfParams[materialId].type;
    // Determine sampled BRDF and the new path direction.

    const mat3 worldTransform = createCoordinateFrame(normal);

    bsdf.normal = toLocal(normal, worldTransform);
    bsdf.wi = toLocal(-gl_WorldRayDirectionEXT, worldTransform);
    bsdf.materialId = materialId;

    const float r1 = rndFloat(hitInfo.rngSeed);
    const float r2 = rndFloat(hitInfo.rngSeed);
    bsdf.unitSample = vec2(r1, r2);

    executeCallableEXT(brdfType, 1);
    hitInfo.sampleDirection = toWorld(bsdf.sampleDirection, worldTransform);
    hitInfo.samplePdf = bsdf.samplePdf;
    hitInfo.bsdfEval = bsdf.eval;

    hitInfo.debugValue = normal;

    // Account for any lights hit.    
    hitInfo.Le = vec3(0.0f);
    if (objId == 3)
    {
        hitInfo.Le = evalAreaLight(position, normal);
    }
}
