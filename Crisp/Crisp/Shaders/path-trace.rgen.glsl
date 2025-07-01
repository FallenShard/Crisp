#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "Common/path-trace-payload.part.glsl"
#include "Common/math-constants.part.glsl"
#include "Common/rng.part.glsl"
#include "Common/warp.part.glsl"
#include "Common/view.part.glsl"

const int kRussianRouletteCutoff = 3;

const int kPayloadIndex = 0;
layout(location = kPayloadIndex) rayPayloadEXT HitInfo hitInfo;

layout(set = 0, binding = 0) uniform accelerationStructureEXT sceneBvh;
layout(set = 0, binding = 1, rgba32f) uniform image2D image;

layout(set = 0, binding = 2) uniform View {
    ViewParameters view;
};

layout(set = 0, binding = 3) uniform IntegratorParams {
    int maxBounces;
    int sampleCount;
    int frameIdx;
    int lightCount;
    int shapeCount;
    int samplingMode;
} integrator;

layout(set = 1, binding = 0, scalar) buffer Vertices {
    vec3 data[];
} vertices;

layout(set = 1, binding = 6, scalar) buffer Normals {
    vec3 data[];
} normals;

layout(set = 1, binding = 1, scalar) buffer Indices {
    uvec3 data[];
} triangles;

#include "Common/path-trace-vertex-pull.part.glsl"

layout(set = 1, binding = 2, scalar) buffer InstanceProps {
    InstanceProperties instanceProps[];
};

layout(set = 1, binding = 4, std430) buffer Lights {
    LightParameters lights[];
};

struct AliasTableElement {
    float tau;
    uint j;
};

layout(set = 1, binding = 5, std430) buffer AliasTables {
    AliasTableElement elements[];
} aliasTable;

void traceRay(inout uint seed, in vec3 rayOrigin, in float tMin, in vec3 rayDirection, in float tMax) {
    hitInfo.rngSeed = seed;
    traceRayEXT(sceneBvh, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, rayOrigin, tMin, rayDirection, tMax, kPayloadIndex);
    seed = hitInfo.rngSeed;
}

void traceShadowRay(inout uint seed, in vec3 rayOrigin, in float tMin, in vec3 rayDirection, in float tMax) {
    hitInfo.rngSeed = seed;
    traceRayEXT(sceneBvh, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, 0, 0, 0, rayOrigin, tMin, rayDirection, tMax, kPayloadIndex);
    seed = hitInfo.rngSeed;
}

void sampleRay(out vec4 origin, out vec4 direction, in vec2 pixelSample) {
    const vec2 ndcSample = pixelSample / vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0; // In [-1, 1].

    origin = view.invV * vec4(0, 0, 0, 1);

    const vec4 target = view.invP * vec4(ndcSample, 0, 1);
    const vec3 rayDirEyeSpace = normalize(target.xyz);

    direction = view.invV * vec4(rayDirEyeSpace, 0.0f);
}

float sampleSurfaceCoord(inout uint seed, in uint meshId, out vec3 position, out vec3 normal) {
    const uint aliasTableOffset = instanceProps[meshId].aliasTableOffset;
    const uint triCount = aliasTable.elements[aliasTableOffset].j;
    
    const uint elemIdx = 1 + rndRange(seed, triCount); // Add 1 to skip the header entry.
    const float rndVal = rndFloat(seed);

    uint sampledTriIdx = elemIdx - 1;
    if (rndVal > aliasTable.elements[aliasTableOffset + elemIdx].tau) {
        sampledTriIdx = aliasTable.elements[aliasTableOffset + elemIdx].j;
    }

    const float r1 = rndFloat(seed);
    const float r2 = rndFloat(seed);
    const vec3 bary = squareToUniformTriangle(vec2(r1, r2));

    const uint triangleOffset = instanceProps[meshId].indexOffset;
    const uvec3 sampledTriangle = triangles.data[triangleOffset + sampledTriIdx];

    position = interpolatePosition(sampledTriangle, bary);
    normal = interpolateNormal(sampledTriangle, bary);

    return aliasTable.elements[aliasTableOffset].tau;
}

vec3 sampleAreaLight(inout uint seed, in uint meshId, in vec3 radiance, in vec3 refPoint, out vec3 shadowRayDir, out float shadowRayLen, out float lightPdf) {
    vec3 samplePos;
    vec3 sampleNormal;
    const float shapePdf = sampleSurfaceCoord(seed, meshId, samplePos, sampleNormal);
    
    shadowRayDir = samplePos - refPoint;

    const float squaredDist = dot(shadowRayDir, shadowRayDir);
    shadowRayLen = sqrt(squaredDist);
    shadowRayDir /= shadowRayLen;

    const float cosThetaO = dot(sampleNormal, -shadowRayDir);
    if (cosThetaO <= 0.0f) {
        return vec3(0.0f);
    }

    lightPdf = shapePdf * squaredDist / cosThetaO;
    return radiance / lightPdf;
}

vec3 sampleUniformLight(inout uint seed, in vec3 refPoint, out vec3 shadowRayDir, out float shadowRayLen, out float lightPdf) {
    const uint lightId = rndRange(seed, integrator.lightCount);
    const float uniformPdf = 1.0f / float(integrator.lightCount);
    
    const vec3 radiance = sampleAreaLight(seed, lights[lightId].meshId, lights[lightId].radiance, refPoint, shadowRayDir, shadowRayLen, lightPdf);
    lightPdf *= uniformPdf;
    return radiance / uniformPdf;
}

float getLightPdf(in int lightId, in vec3 hitVector, in vec3 hitNormal) {
    const int meshId = lights[lightId].meshId;
    const uint aliasTableOffset = instanceProps[meshId].aliasTableOffset;
    const float shapePdf = aliasTable.elements[aliasTableOffset].tau;

    const float squaredDist = dot(hitVector, hitVector);

    const float uniformPdf = 1.0f / float(integrator.lightCount);
    return uniformPdf * shapePdf * squaredDist / dot(hitNormal, -normalize(hitVector));
}

vec3 computeRadianceDirectLighting(inout uint seed) {
    // Sample a point on the screen and transform it into a ray.
    const vec2 subpixelSample = vec2(rndFloat(seed), rndFloat(seed));
    const vec2 pixelSample = vec2(gl_LaunchIDEXT.xy) + subpixelSample;

    vec4 rayOrigin;
    vec4 rayDirection;
    sampleRay(rayOrigin, rayDirection, pixelSample);
    const float tMin = 1e-4;
    const float tMax = view.nearFar[1];

    // Accumulated radiance L for this path.
    vec3 L = vec3(0.0f);

    traceRay(seed, rayOrigin.xyz, tMin, rayDirection.xyz, tMax);

    if (hitInfo.tHit == -1.0)
    {
        return L;
    }

    L += hitInfo.Le;

    const vec3 p = hitInfo.position;
    const vec3 n = hitInfo.normal;
    const vec3 f = hitInfo.bsdfEval;

    vec3 shadowRayDir;
    float shadowRayLen;
    float lightPdf;
    const vec3 radiance = sampleUniformLight(seed, p, shadowRayDir, shadowRayLen, lightPdf);
    traceShadowRay(seed, p, 1e-5, shadowRayDir, shadowRayLen - 1e-5);

    if (hitInfo.tHit <= 0)
    {
        const vec3 brdfEval = f * InvPI * dot(n, shadowRayDir);
        L += radiance * brdfEval;
    }

    return L;
}

float powerHeuristic(const float fPdf, const float gPdf) {
    const float fPdf2 = fPdf * fPdf;
    const float gPdf2 = gPdf * gPdf;
    return fPdf2 / (fPdf2 + gPdf2);
}

vec3 computeRadianceMis(inout uint seed) {
    // Sample a point on the screen and transform it into a ray.
    const vec2 subpixelSample = vec2(rndFloat(seed), rndFloat(seed));
    const vec2 pixelSample = vec2(gl_LaunchIDEXT.xy) + subpixelSample;

    vec4 rayOrigin;
    vec4 rayDirection;
    sampleRay(rayOrigin, rayDirection, pixelSample);
    const float tMin = 1e-4;
    const float tMax = view.nearFar[1]; 

    // Accumulated radiance L for this path.
    vec3 L = vec3(0.0f);

    traceRay(seed, rayOrigin.xyz, tMin, rayDirection.xyz, tMax);

    if (hitInfo.tHit == -1.0) {
        return L;
    }

    L += hitInfo.Le;

    const vec3 p = hitInfo.position;
    const vec3 n = hitInfo.normal;
    const vec3 f = hitInfo.bsdfEval;

    // BRDF sampling.
    {
        const float brdfPdf = hitInfo.samplePdf;
        traceRay(seed, p, tMin, hitInfo.sampleDirection, tMax);

        if (hitInfo.lightId != -1) {
            const float lightPdf = getLightPdf(hitInfo.lightId, hitInfo.position - p, hitInfo.normal);
            L += f * hitInfo.Le * powerHeuristic(brdfPdf, lightPdf);
        }
    }

    // Light sampling.
    vec3 shadowRayDir;
    float shadowRayLen;
    float lightPdf;
    const vec3 radiance = sampleUniformLight(seed, p, shadowRayDir, shadowRayLen, lightPdf);
    traceShadowRay(seed, p, 1e-5, shadowRayDir, shadowRayLen - 1e-5);

    if (hitInfo.tHit <= 0) // The shadow ray has missed.
    {
        const float brdfPdf = InvPI * dot(n, shadowRayDir);
        const vec3 brdfEval = f * brdfPdf;
        L += radiance * brdfEval * powerHeuristic(lightPdf, brdfPdf);
    }

    return L;
}

vec3 computeRadianceMisPt(inout uint seed) {
    // Sample a point on the screen and transform it into a ray.
    const vec2 subpixelSample = vec2(rndFloat(seed), rndFloat(seed));
    const vec2 pixelSample = vec2(gl_LaunchIDEXT.xy) + subpixelSample;

    vec4 rayOrigin;
    vec4 rayDirection;
    sampleRay(rayOrigin, rayDirection, pixelSample);
    const float tMin = 1e-4;
    const float tMax = view.nearFar[1];

    // Accumulated radiance L for this path.
    vec3 L = vec3(0.0f);
    vec3 throughput = vec3(1.0f);

    bool specularBounce = false;
    int bounceCount = 0;
    while (bounceCount < integrator.maxBounces) {
        traceRay(seed, rayOrigin.xyz, tMin, rayDirection.xyz, tMax);
        if (bounceCount == 0 || specularBounce) {
            if (hitInfo.tHit >= tMin) {
                // Accumulate any emission from the hit surface (e.g. we hit a light).
                L += throughput * hitInfo.Le;
            } else { // The ray missed, evaluate environment lighting and exit the loop. 
                // L += throughput * texture(environmentMap, rayDirection);
                break;
            }
        }

        if (hitInfo.tHit < tMin) {
            break;
        }

        const vec3 p = hitInfo.position;
        const vec3 n = hitInfo.normal;
        const vec3 f = hitInfo.bsdfEval;
        const float brdfPdf = hitInfo.samplePdf;
        const vec3 rayDir = hitInfo.sampleDirection;
        specularBounce = hitInfo.sampleLobeType == kLobeTypeDelta;

        // If the bounce wasn't a delta bounce (glass/mirror), do light sampling.
        if (!specularBounce) {
            vec3 shadowRayDir;
            float shadowRayLen;
            float lightPdf;
            const vec3 radiance = sampleUniformLight(seed, p, shadowRayDir, shadowRayLen, lightPdf);
            traceShadowRay(seed, p, 1e-5, shadowRayDir, shadowRayLen - 1e-5);

            if (hitInfo.tHit <= 0) { // The shadow ray has missed.
                const float brdfPdf = InvPI * dot(n, shadowRayDir); // This is only correct for Diffuse BRDFs.
                L += throughput * f * radiance * brdfPdf * powerHeuristic(lightPdf, brdfPdf);
            }
        }

        // If the light isn't a delta light (point or directional), do bsdf sampling.
        {
            traceRay(seed, p, tMin, rayDir, tMax);

            if (hitInfo.lightId != -1) {
                const float lightPdf = getLightPdf(hitInfo.lightId, hitInfo.position - p, hitInfo.normal);
                L += throughput * f * hitInfo.Le * powerHeuristic(brdfPdf, lightPdf);
            }
        }

        // Adjust throughput for the hit surface.
        throughput *= f; // equal to f(wi) * cos(wo) / pdf(wo).

        // Setup the next ray.
        rayOrigin.xyz    = p;
        rayDirection.xyz = rayDir;


        if (++bounceCount > kRussianRouletteCutoff) { // Cut the path tracing with Russian roulette.
            const float maxCoeff = max(throughput.x, max(throughput.y, throughput.z));
            const float q = 1.0f - min(maxCoeff, 0.99f);
            if (rndFloat(seed) > q) {
                throughput /= 1.0f - q;
            } else {
                break;
            }
        }
    }

    return L;
}

vec3 computeRadiance(inout uint seed) {
    // Sample a point on the screen and transform it into a ray.
    const vec2 subpixelSample = vec2(rndFloat(seed), rndFloat(seed));
    const vec2 pixelSample = vec2(gl_LaunchIDEXT.xy) + subpixelSample;
    
    vec4 rayOrigin;
    vec4 rayDirection;
    sampleRay(rayOrigin, rayDirection, pixelSample);
    const float tMin = 1e-4;
    const float tMax = view.nearFar[1];

    // Accumulated radiance L for this path.
    vec3 L = vec3(0.0f);

    // Throughput of the current path, modulated by encountered BRDFs.
    vec3 throughput = vec3(1.0f);

    vec3 debugColor = vec3(0.0f);
    int bounceCount = 0;
    while (bounceCount < integrator.maxBounces) {
        traceRay(seed, rayOrigin.xyz, tMin, rayDirection.xyz, tMax);
        if (hitInfo.tHit >= tMin) {
            // Accumulate any emission from the hit surface (e.g. we hit a light).
            L += throughput * hitInfo.Le;

            // Adjust throughput for the hit surface.
            throughput *= hitInfo.bsdfEval; // equal to f(wi) * cos(wo) / pdf(wo).

            // Setup the next ray.
            rayOrigin.xyz    = hitInfo.position;
            rayDirection.xyz = hitInfo.sampleDirection;
        } else { // The ray missed, evaluate environment lighting and exit the loop.
            // L += throughput * texture(environmentMap, rayDirection);
            break;
        }

        if (++bounceCount > kRussianRouletteCutoff) { // Cut the path tracing with Russian roulette.
            const float maxCoeff = max(throughput.x, max(throughput.y, throughput.z));
            const float q = 1.0f - min(maxCoeff, 0.99f);
            if (rndFloat(seed) > q) {
                throughput /= 1.0f - q;
            } else {
                break;
            }
        }
    }

    return L;
}

void main() {
    uint seed = tea(gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x, integrator.frameIdx);

    vec3 L = vec3(0.0f);
    const uint sampleCount = integrator.sampleCount;
    for (uint i = 0; i < sampleCount; ++i) {
        if (integrator.samplingMode == 0) {
            L += computeRadianceDirectLighting(seed);
        } else if (integrator.samplingMode == 1) {
            L += computeRadiance(seed);
        } else {
            L += computeRadianceMisPt(seed);
        }
    }
    L /= sampleCount;

    if (integrator.frameIdx > 0) {
       const float t = 1.0f / (integrator.frameIdx + 1);
       const vec3 prevVal = imageLoad(image, ivec2(gl_LaunchIDEXT.xy)).xyz;
       L = mix(prevVal, L, t);
    }

    imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(L, 1.0));
}
