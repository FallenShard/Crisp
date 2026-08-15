#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_query : require
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

layout(set = 1, binding = 0) uniform accelerationStructureEXT sceneBvh;
layout(set = 1, binding = 1, rgba32f) uniform image2D image;

layout(set = 1, binding = 2) uniform View {
    ViewParameters view;
};

layout(set = 1, binding = 3) uniform IntegratorParams {
    int maxBounces;
    int sampleCount;
    int frameIdx;
    int lightCount;
    int shapeCount;
    int samplingMode;
} integrator;

#include "Common/path-trace-scene.part.glsl"
#include "Common/path-trace-vertex-pull.part.glsl"
#include "Integrators/brdf-eval.part.glsl"

BrdfEval evaluateBrdfWorldSpace(
    vec3 normal,
    vec3 wi,
    vec3 wo,
    uint materialId)
{
    const mat3 coordinateFrame = createCoordinateFrame(normal);
    const mat3 worldToLocal = transpose(coordinateFrame);
    return evaluateBrdf(
        scene.materials.data[materialId],
        worldToLocal * wi,
        worldToLocal * wo);
}

void traceRay(inout uint seed, in vec3 rayOrigin, in float tMin, in vec3 rayDirection, in float tMax) {
    hitInfo.rngSeed = seed;
    traceRayEXT(sceneBvh, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, rayOrigin, tMin, rayDirection, tMax, kPayloadIndex);
    seed = hitInfo.rngSeed;
}

bool traceShadowRay(in vec3 rayOrigin, in float tMin, in vec3 rayDirection, in float tMax) {
    rayQueryEXT rayQuery;
    rayQueryInitializeEXT(
        rayQuery,
        sceneBvh,
        gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT,
        0xFF,
        rayOrigin,
        tMin,
        rayDirection,
        tMax);
    rayQueryProceedEXT(rayQuery);
    return rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT;
}

void sampleRay(out vec4 origin, out vec4 direction, in vec2 pixelSample) {
    const vec2 ndcSample = pixelSample / vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0; // In [-1, 1].

    origin = view.invV * vec4(0, 0, 0, 1);

    const vec4 target = view.invP * vec4(ndcSample, 0, 1);
    const vec3 rayDirEyeSpace = normalize(target.xyz);

    direction = view.invV * vec4(rayDirEyeSpace, 0.0f);
}

float sampleSurfaceCoord(inout uint seed, in uint meshId, out vec3 position, out vec3 normal) {
    const uint aliasTableOffset = scene.instances.data[meshId].aliasTableOffset;
    const uint triCount = scene.aliasTable.data[aliasTableOffset].j;
    
    const uint elemIdx = 1 + rndRange(seed, triCount); // Add 1 to skip the header entry.
    const float rndVal = rndFloat(seed);

    uint sampledTriIdx = elemIdx - 1;
    if (rndVal > scene.aliasTable.data[aliasTableOffset + elemIdx].tau) {
        sampledTriIdx = scene.aliasTable.data[aliasTableOffset + elemIdx].j;
    }

    const float r1 = rndFloat(seed);
    const float r2 = rndFloat(seed);
    const vec3 bary = squareToUniformTriangle(vec2(r1, r2));

    const uint triangleOffset = scene.instances.data[meshId].indexOffset;
    const uvec3 sampledTriangle = scene.triangles.data[triangleOffset + sampledTriIdx];

    position = interpolatePosition(sampledTriangle, bary);
    normal = interpolateNormal(sampledTriangle, bary);

    return scene.aliasTable.data[aliasTableOffset].tau;
}

vec3 sampleAreaLight(inout uint seed, in uint meshId, in vec3 radiance, in vec3 refPoint, out vec3 shadowRayDir, out float shadowRayLen, out float lightPdf) {
    lightPdf = 0.0f;

    vec3 samplePos;
    vec3 sampleNormal;
    const float shapePdf = sampleSurfaceCoord(seed, meshId, samplePos, sampleNormal);
    
    shadowRayDir = samplePos - refPoint;

    const float squaredDist = dot(shadowRayDir, shadowRayDir);
    shadowRayLen = sqrt(squaredDist);
    if (shadowRayLen <= 0.0f) {
        shadowRayDir = vec3(0.0f);
        return vec3(0.0f);
    }
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
    
    const vec3 radiance = sampleAreaLight(
        seed,
        scene.lights.data[lightId].meshId,
        scene.lights.data[lightId].radiance,
        refPoint,
        shadowRayDir,
        shadowRayLen,
        lightPdf);
    lightPdf *= uniformPdf;
    return radiance / uniformPdf;
}

float getLightPdf(in int lightId, in vec3 hitVector, in vec3 hitNormal) {
    const int meshId = scene.lights.data[lightId].meshId;
    const uint aliasTableOffset = scene.instances.data[meshId].aliasTableOffset;
    const float shapePdf = scene.aliasTable.data[aliasTableOffset].tau;

    const float squaredDist = dot(hitVector, hitVector);
    const float cosTheta = dot(hitNormal, -normalize(hitVector));
    if (cosTheta <= 0.0f) {
        return 0.0f;
    }

    const float uniformPdf = 1.0f / float(integrator.lightCount);
    return uniformPdf * shapePdf * squaredDist / cosTheta;
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

    if (hitInfo.tHit == -1.0) {
        return L;
    }

    L += hitInfo.Le;

    const vec3 p = hitInfo.position;
    const vec3 n = hitInfo.normal;
    const vec3 wi = -rayDirection.xyz;
    const uint materialId = hitInfo.materialId;

    vec3 shadowRayDir;
    float shadowRayLen;
    float lightPdf;
    const vec3 radiance = sampleUniformLight(seed, p, shadowRayDir, shadowRayLen, lightPdf);
    if (lightPdf > 0.0f) {
        if (!traceShadowRay(p, 1e-5, shadowRayDir, shadowRayLen - 1e-5)) {
            const BrdfEval lightDirectionBrdf =
                evaluateBrdfWorldSpace(n, wi, shadowRayDir, materialId);
            L += radiance * lightDirectionBrdf.f;
        }
    }

    return L;
}

float powerHeuristic(const float fPdf, const float gPdf) {
    const float fPdfSq = fPdf * fPdf;
    const float gPdfSq = gPdf * gPdf;
    const float denominator = fPdfSq + gPdfSq;
    return denominator > 0.0f ? fPdfSq / denominator : 0.0f;
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
    const vec3 sampleWeight = hitInfo.sampleWeight;
    const vec3 wi = -rayDirection.xyz;
    const uint materialId = hitInfo.materialId;
    const bool deltaSample = hitInfo.sampleLobeType == kLobeTypeDelta;

    // BRDF sampling.
    {
        const float samplePdf = hitInfo.samplePdf;
        traceRay(seed, p, tMin, hitInfo.sampleDirection, tMax);

        if (hitInfo.lightId != -1) {
            const float lightPdf = getLightPdf(hitInfo.lightId, hitInfo.position - p, hitInfo.normal);
            const float misWeight = deltaSample ? 1.0f : powerHeuristic(samplePdf, lightPdf);
            L += sampleWeight * hitInfo.Le * misWeight;
        }
    }

    // Light sampling.
    vec3 shadowRayDir;
    float shadowRayLen;
    float lightPdf;
    const vec3 radiance = sampleUniformLight(seed, p, shadowRayDir, shadowRayLen, lightPdf);
    if (lightPdf > 0.0f) {
        if (!traceShadowRay(p, 1e-5, shadowRayDir, shadowRayLen - 1e-5)) {
            const BrdfEval lightDirectionBrdf =
                evaluateBrdfWorldSpace(n, wi, shadowRayDir, materialId);
            L += radiance * lightDirectionBrdf.f * powerHeuristic(lightPdf, lightDirectionBrdf.pdf);
        }
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

    vec3 prevPosition = vec3(0.0f);
    float prevSamplePdf = 0.0f;
    bool prevWasDelta = false;

    int bounceCount = 0;
    while (true) {
        traceRay(seed, rayOrigin.xyz, tMin, rayDirection.xyz, tMax);
        if (hitInfo.tHit < tMin) {
            // The ray missed; evaluate environment lighting here once it is supported.
            // L += throughput * texture(environmentMap, rayDirection);
            break;
        }

        if (hitInfo.lightId != -1) {
            float misWeight = 1.0f;
            if (bounceCount > 0 && !prevWasDelta) {
                const float lightPdf =
                    getLightPdf(hitInfo.lightId, hitInfo.position - prevPosition, hitInfo.normal);
                misWeight = powerHeuristic(prevSamplePdf, lightPdf);
            }
            L += throughput * hitInfo.Le * misWeight;
        }

        if (bounceCount >= integrator.maxBounces) {
            break;
        }

        const vec3 p = hitInfo.position;
        const vec3 n = hitInfo.normal;
        const vec3 sampleWeight = hitInfo.sampleWeight;
        const float samplePdf = hitInfo.samplePdf;
        const vec3 rayDir = hitInfo.sampleDirection;
        const vec3 wi = -rayDirection.xyz;
        const uint materialId = hitInfo.materialId;
        const bool isDelta = hitInfo.sampleLobeType == kLobeTypeDelta;

        // If the bounce wasn't a delta bounce (glass/mirror), do light sampling.
        if (!isDelta) {
            vec3 shadowRayDir;
            float shadowRayLen;
            float lightPdf;
            const vec3 radiance = sampleUniformLight(seed, p, shadowRayDir, shadowRayLen, lightPdf);
            if (lightPdf > 0.0f) {
                if (!traceShadowRay(p, 1e-5, shadowRayDir, shadowRayLen - 1e-5)) {
                    const BrdfEval lightDirectionBrdf =
                        evaluateBrdfWorldSpace(n, wi, shadowRayDir, materialId);
                    L += throughput * radiance * lightDirectionBrdf.f *
                        powerHeuristic(lightPdf, lightDirectionBrdf.pdf);
                }
            }
        }

        // Adjust throughput for the hit surface.
        throughput *= sampleWeight;

        prevPosition = p;
        prevSamplePdf = samplePdf;
        prevWasDelta = isDelta;

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
            throughput *= hitInfo.sampleWeight; // equal to f(wi) * cos(wo) / pdf(wo).

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
    if (integrator.lightCount <= 0) {
        imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(L, 1.0));
        return;
    }

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
