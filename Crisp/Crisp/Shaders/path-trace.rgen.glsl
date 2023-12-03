#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "Parts/path-tracer-payload.part.glsl"
#include "Parts/math-constants.part.glsl"
#include "Parts/rng.part.glsl"
#include "Parts/warp.part.glsl"

const int kPayloadIndex = 0;

layout(location = 0) rayPayloadEXT HitInfo hitInfo;

layout(set = 0, binding = 0) uniform accelerationStructureEXT sceneBvh;
layout(set = 0, binding = 1, rgba32f) uniform image2D image;

layout(set = 0, binding = 2) uniform Camera
{
    mat4 V;
    mat4 P;
    mat4 invV;
    mat4 invP;
    vec2 screenSize;
    vec2 nearFar;
} camera;

layout(set = 0, binding = 3) uniform IntegratorParameters
{
    int maxBounces;
    int sampleCount;
    int frameIdx;
    int lightCount;
    int shapeCount;
    int useEms;
} integrator;

layout(set = 1, binding = 0, scalar) buffer Vertices
{
    float v[];
} vertices[6];

layout(set = 1, binding = 1, scalar) buffer Indices
{
    uint i[];
} indices[6];

layout(set = 1, binding = 2, scalar) buffer InstanceProps
{
    InstanceProperties instanceProps[];
};

layout(set = 1, binding = 4, std430) buffer Lights
{
    LightParameters lights[];
};

struct AliasTableElement
{
    float tau;
    uint j;
};

layout(set = 1, binding = 5, std430) buffer AliasTables
{
    AliasTableElement elements[];
} aliasTable;

void sampleRay(out vec4 origin, out vec4 direction, in vec2 pixelSample)
{
    const vec2 ndcSample = pixelSample / vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0; // In [-1, 1].

    origin = camera.invV * vec4(0, 0, 0, 1);

    const vec4 target = camera.invP * vec4(ndcSample, 0, 1);
    const vec3 rayDirEyeSpace = normalize(target.xyz);

    direction = camera.invV * vec4(rayDirEyeSpace, 0.0f);
}

// vec3 computeRadianceMis(inout uint seed)
// {
//     // Sample a point on the screen and transform it into a ray.
//     const vec2 subpixelSample = vec2(rndFloat(seed), rndFloat(seed));
//     const vec2 pixelSample = vec2(gl_LaunchIDEXT.xy) + subpixelSample;
    
//     vec4 rayOrigin;
//     vec4 rayDirection;
//     sampleRay(rayOrigin, rayDirection, pixelSample);
//     const float tMin = 1e-4;
//     const float tMax = camera.nearFar[1];

//     // Accumulated radiance L for this path.
//     vec3 L = vec3(0.0f);

//     // Throughput of the current path, modulated by encountered BRDFs.
//     vec3 throughput = vec3(1.0f);

//     vec3 debugColor = vec3(0.0f);
//     int bounceCount = 0;
//     bool specularBounce = false;
//     while (bounceCount < integrator.maxBounces)
//     {
//         hitInfo.bounceCount = bounceCount;
//         hitInfo.rngSeed = seed;
//         traceRayEXT(sceneBvh, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, rayOrigin.xyz, tMin, rayDirection.xyz, tMax, kPayloadIndex);
//         seed = hitInfo.rngSeed;

//         if (bounceCount == 0 || specularBounce) {
//             if (hitInfo.tHit >= tMin) {
//                 // Accumulate any emission from the hit surface (e.g. we hit a light).
//                 L += throughput * hitInfo.Le;
//             } else {
//                 // L += throughput * texture(environmentMap, rayDirection);
//             }
//         }

//         if (!(hitInfo.tHit >= tMin) || bounceCount >= integrator.maxBounces) {
//             break;
//         }

//         if (hitInfo.sampleLobeType != kLobeDelta) {
//             const vec3 lightMis = uniformSampleOneLight(seed, rayOrigin, rayDirection);
//             L += throughput * lightMis;
//         }

//         throughput *= hitInfo.bsdfEval;

//         specularBounce = hitInfo.sampleLobeType == Lobe::Delta;

//         // if (hitInfo.tHit >= tMin)
//         // {
//         //     // Adjust throughput for the hit surface.
//         //     throughput *= hitInfo.bsdfEval; // equal to f(wi) * cos(wo) / pdf(wo).

//         // Setup the next ray.
//         rayOrigin.xyz    = hitInfo.position;
//         rayDirection.xyz = hitInfo.sampleDirection;

//         //     debugColor = hitInfo.debugValue.xyz;
//         // }
//         // else // The ray missed, evaluate environment lighting and exit the loop.
//         // {
//         //     // 
//         //     break;
//         // }

//         if (++bounceCount > 10) // Cut the path tracing with Russian roulette.
//         {
//             const float maxCoeff = max(throughput.x, max(throughput.y, throughput.z));
//             const float q = 1.0f - min(maxCoeff, 0.99f);
//             if (rndFloat(seed) > q)
//                 throughput /= 1.0f - q;
//             else
//                 break;
//         }
//     }

//     return L;
// }

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

float sampleSurfaceCoord(inout uint seed, in uint meshId, out vec3 position, out vec3 normal)
{
    const uint aliasTableOffset = instanceProps[meshId].aliasTableOffset;
    const uint triCount = aliasTable.elements[aliasTableOffset].j;
    
    //const uint triCount = instanceProps[meshId].aliasTableCount;
    const uint elemIdx = 1 + rndRange(seed, triCount); // Add 1 to skip the header entry.
    const float rndVal = rndFloat(seed);

    uint sampledTriIdx = elemIdx - 1;
    if (rndVal > aliasTable.elements[aliasTableOffset + elemIdx].tau)
    {
        sampledTriIdx = aliasTable.elements[aliasTableOffset + elemIdx].j;
    }

    const float r1 = rndFloat(seed);
    const float r2 = rndFloat(seed);
    const vec3 bary = squareToUniformTriangle(vec2(r1, r2));

    const ivec3 sampledTriangle = ivec3(
        indices[meshId].i[3 * sampledTriIdx + 0],
        indices[meshId].i[3 * sampledTriIdx + 1],
        indices[meshId].i[3 * sampledTriIdx + 2]);

    position = getPosition(meshId, sampledTriangle, bary);
    normal = getNormal(meshId, sampledTriangle, bary);
    return aliasTable.elements[aliasTableOffset].tau;
}

vec3 sampleAreaLight(inout uint seed, in uint meshId, in vec3 radiance, in vec3 refPoint, out vec3 shadowRayDir, out float lightPdf) {
    vec3 samplePos;
    vec3 sampleNormal;
    const float shapePdf = sampleSurfaceCoord(seed, meshId, samplePos, sampleNormal);
    
    const vec3 shadowRayOrg = refPoint;
    shadowRayDir = samplePos - refPoint;

    const float squaredDist = dot(shadowRayDir, shadowRayDir);
    const float dist = sqrt(squaredDist);
    shadowRayDir /= dist;
    hitInfo.rngSeed = seed;
    traceRayEXT(sceneBvh, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, shadowRayOrg.xyz, 1e-6, shadowRayDir.xyz, dist * 0.999, kPayloadIndex);
    seed = hitInfo.rngSeed;

    lightPdf = shapePdf * squaredDist / dot(sampleNormal, -shadowRayDir);
    return radiance / lightPdf;
}

vec3 sampleUniformLight(inout uint seed, in vec3 refPoint, out vec3 shadowRayDir, out float lightPdf) {
    const uint lightId = rndRange(seed, integrator.lightCount);
    const float uniformPdf = 1.0f / float(integrator.lightCount);
    const vec3 radiance = sampleAreaLight(seed, lights[lightId].meshId, lights[lightId].radiance, refPoint, shadowRayDir, lightPdf);
    lightPdf *= uniformPdf;
    return radiance / uniformPdf;
}

// vec3 sampleLight(inout uint seed, in vec3 refPoint, out vec3 shadowRayDir)
// {
//     const vec3 center = vec3(0.0f, 0.0f, 0.0f);
//     const float radius = 0.5f;

//     const float invRad2 = 1.0f / (radius * radius);
//     // Uniform sampling
//     vec3 q = squareToUniformSphere(vec2(rndFloat(seed), rndFloat(seed)));

//     vec3 samplePos = center + radius * q;
//     vec3 sampleNormal = q;
//     const float shapePdf = invRad2 * squareToUniformSpherePdf();

//     const vec3 shadowRayOrg = refPoint;
//     shadowRayDir = samplePos - refPoint;

//     const float squaredDist = dot(shadowRayDir, shadowRayDir);
//     const float dist = sqrt(squaredDist);
//     shadowRayDir /= dist;
//     hitInfo.rngSeed = seed;
//     traceRayEXT(sceneBvh, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, shadowRayOrg.xyz, 1e-6, shadowRayDir.xyz, dist * 0.999, kPayloadIndex);
//     seed = hitInfo.rngSeed;

//     const float lightSamplePdf = shapePdf * squaredDist / dot(sampleNormal, -shadowRayDir);
//     return vec3(5.0f) / lightSamplePdf;
// }

vec3 computeRadianceDirectLighting(inout uint seed)
{
    // Sample a point on the screen and transform it into a ray.
    const vec2 subpixelSample = vec2(rndFloat(seed), rndFloat(seed));
    const vec2 pixelSample = vec2(gl_LaunchIDEXT.xy) + subpixelSample;

    vec4 rayOrigin;
    vec4 rayDirection;
    sampleRay(rayOrigin, rayDirection, pixelSample);
    const float tMin = 1e-4;
    const float tMax = camera.nearFar[1];

    // Accumulated radiance L for this path.
    vec3 L = vec3(0.0f);

    int bounceCount = 0;
    hitInfo.bounceCount = bounceCount;
    hitInfo.rngSeed = seed;
    traceRayEXT(sceneBvh, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, rayOrigin.xyz, tMin, rayDirection.xyz, tMax, kPayloadIndex);
    seed = hitInfo.rngSeed;

    if (hitInfo.tHit == -1.0)
    {
        return L;
    }

    L += hitInfo.Le;

    vec3 shadowRayDir;
    float lightPdf;
    const vec3 radiance = sampleUniformLight(seed, hitInfo.position, shadowRayDir, lightPdf);
    const vec3 brdfEval = hitInfo.bsdfEval * InvPI * dot(hitInfo.debugValue, shadowRayDir);

    if (hitInfo.tHit <= 0)
    {
        L += radiance * brdfEval;
    }

    return L;
}

vec3 computeRadiance(inout uint seed)
{
    // Sample a point on the screen and transform it into a ray.
    const vec2 subpixelSample = vec2(rndFloat(seed), rndFloat(seed));
    const vec2 pixelSample = vec2(gl_LaunchIDEXT.xy) + subpixelSample;
    
    vec4 rayOrigin;
    vec4 rayDirection;
    sampleRay(rayOrigin, rayDirection, pixelSample);
    const float tMin = 1e-4;
    const float tMax = camera.nearFar[1];

    // Accumulated radiance L for this path.
    vec3 L = vec3(0.0f);

    // Throughput of the current path, modulated by encountered BRDFs.
    vec3 throughput = vec3(1.0f);

    vec3 debugColor = vec3(0.0f);
    int bounceCount = 0;
    while (bounceCount < integrator.maxBounces)
    {
        hitInfo.bounceCount = bounceCount;
        hitInfo.rngSeed = seed;
        traceRayEXT(sceneBvh, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, rayOrigin.xyz, tMin, rayDirection.xyz, tMax, kPayloadIndex);
        seed = hitInfo.rngSeed;

        if (hitInfo.tHit >= tMin)
        {
            // Accumulate any emission from the hit surface (e.g. we hit a light).
            L += throughput * hitInfo.Le;

            // Adjust throughput for the hit surface.
            throughput *= hitInfo.bsdfEval; // equal to f(wi) * cos(wo) / pdf(wo).

            // Setup the next ray.
            rayOrigin.xyz    = hitInfo.position;
            rayDirection.xyz = hitInfo.sampleDirection;

            debugColor = hitInfo.debugValue.xyz;
        }
        else // The ray missed, evaluate environment lighting and exit the loop.
        {
            // L += throughput * texture(environmentMap, rayDirection);
            break;
        }

        if (bounceCount == 0)
            debugColor.r = hitInfo.debugValue.x;
        else
            debugColor.g = hitInfo.debugValue.x;

        if (++bounceCount > 10) // Cut the path tracing with Russian roulette.
        {
            const float maxCoeff = max(throughput.x, max(throughput.y, throughput.z));
            const float q = 1.0f - min(maxCoeff, 0.99f);
            if (rndFloat(seed) > q)
                throughput /= 1.0f - q;
            else
                break;
        }
    }

    return L;
}

void main() 
{
    uint seed = tea(gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x, integrator.frameIdx);

    vec3 L = vec3(0.0f);
    const uint sampleCount = integrator.sampleCount;
    for (uint i = 0; i < sampleCount; ++i)
    {
        if (integrator.useEms == 1) {
            L += computeRadianceDirectLighting(seed);
        }
        else {
            L += computeRadiance(seed);
        }
    }
    L /= sampleCount;

    if (integrator.frameIdx > 0)
    {
       const float t = 1.0f / (integrator.frameIdx + 1);
       const vec3 prevVal = imageLoad(image, ivec2(gl_LaunchIDEXT.xy)).xyz;
       imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(mix(prevVal, L, t), 1.0));
    }
    else
    {
       imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(L, 1.0));
    }
}
