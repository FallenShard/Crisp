#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_descriptor_heap : require
#extension GL_EXT_ray_query : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "Core/heap-slots.part.glsl"
#include "Core/types.part.glsl"
#include "../Common/math-constants.part.glsl"
#include "../Common/rng.part.glsl"
#include "../Common/warp.part.glsl"
#include "../Common/view.part.glsl"

const int kRussianRouletteCutoff = 3;

const int kPayloadIndex = 0;
layout(location = kPayloadIndex) rayPayloadEXT HitInfo hitInfo;

// This doesn't work yet in a descriptor_heap: Nvidia driver bug.
layout(set = 1, binding = 0) uniform accelerationStructureEXT sceneBvh;
layout(descriptor_heap, descriptor_stride = 64, rgba32f) uniform image2D heapStorageImages[];

layout(descriptor_heap, descriptor_stride = 64) uniform View {
    ViewParameters params;
}
heapViews[];

layout(descriptor_heap, descriptor_stride = 64) uniform IntegratorParams {
    int maxBounces;
    int sampleCount;
    int frameIdx;
    int sampleOffset;
    uint seed;
    int reconstructionFilter;
    int lightCount;
    int shapeCount;
    int samplingMode;
    int environmentEnabled;
    int environmentWidth;
    int environmentHeight;
    float environmentScale;
}
heapIntegrators[];

#define image heapStorageImages[kImageSlot]
#define view heapViews[kViewSlot].params
#define integrator heapIntegrators[kIntegratorSlot]
#define environmentMap heapTexture2Ds[kEnvironmentSlot]
#define environmentSampler heapSamplers[kEnvironmentSamplerSlot]
// bsdf-eval.part.glsl evaluates kBsdfOpenPbr inline for next-event estimation, and that reaches the GGX
// directional-albedo table even when no compensation mode is active.
#define CRISP_GGX_ALBEDO_LUT sampler2D(heapTexture2Ds[kGgxAlbedoLutSlot], heapSamplers[kGgxAlbedoLutSamplerSlot])

#include "Core/scene-addresses.part.glsl"
#include "Core/intersection.part.glsl"
#include "Textures/material-texture.part.glsl"
#include "BSDFs/bsdf-eval.part.glsl"

BsdfEval evaluateBsdfWorldSpace(vec3 normal, vec3 wi, vec3 wo, uint materialId, vec2 texCoord) {
    const mat3 coordinateFrame = createCoordinateFrame(normal);
    const mat3 worldToLocal = transpose(coordinateFrame);
    return evaluateBsdf(scene.materials.data[materialId], texCoord, worldToLocal * wi, worldToLocal * wo);
}

#include "Core/tracing.part.glsl"
#include "Cameras/perspective.part.glsl"
#include "Lights/light-sampling.part.glsl"

vec2 samplePixelPosition(inout Sampler rng) {
    setDimension(rng, kDimPixelFilter);
    if (integrator.reconstructionFilter == 0) {
        return vec2(gl_LaunchIDEXT.xy) + next2D(rng);
    }
    return vec2(gl_LaunchIDEXT.xy) + vec2(0.5f);
}

vec3 computeRadianceDirectLighting(inout Sampler rng) {
    // Sample a point on the screen and transform it into a ray.
    const vec2 pixelSample = samplePixelPosition(rng);

    vec4 rayOrigin;
    vec4 rayDirection;
    sampleRay(rayOrigin, rayDirection, pixelSample);
    const float tMin = 1e-4;
    const float tMax = view.nearFar[1];

    // Accumulated radiance L for this path.
    vec3 L = vec3(0.0f);

    traceRay(rng, kDimBounceBase, rayOrigin.xyz, tMin, rayDirection.xyz, tMax);

    if (hitInfo.tHit == -1.0) {
        return evaluateEnvironment(rayDirection.xyz);
    }

    L += hitInfo.Le;

    const vec3 p = hitInfo.position;
    const vec3 n = hitInfo.normal;
    const vec3 wi = -rayDirection.xyz;
    const uint materialId = hitInfo.materialId;
    const vec2 texCoord = hitInfo.texCoord;

    vec3 shadowRayDir;
    float shadowRayLen;
    float lightPdf;
    bool lightIsDelta;
    setDimension(rng, kDimBounceBase + kDimLight);
    const vec3 radiance = sampleUniformLight(rng, p, shadowRayDir, shadowRayLen, lightPdf, lightIsDelta);
    if (lightPdf > 0.0f) {
        if (!traceShadowRay(p, 1e-5, shadowRayDir, shadowRayLen - 1e-5)) {
            const BsdfEval lightDirectionBsdf = evaluateBsdfWorldSpace(n, wi, shadowRayDir, materialId, texCoord);
            L += radiance * lightDirectionBsdf.f;
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

vec3 computeRadianceMis(inout Sampler rng) {
    // Sample a point on the screen and transform it into a ray.
    const vec2 pixelSample = samplePixelPosition(rng);

    vec4 rayOrigin;
    vec4 rayDirection;
    sampleRay(rayOrigin, rayDirection, pixelSample);
    const float tMin = 1e-4;
    const float tMax = view.nearFar[1];

    // Accumulated radiance L for this path.
    vec3 L = vec3(0.0f);

    traceRay(rng, kDimBounceBase, rayOrigin.xyz, tMin, rayDirection.xyz, tMax);

    if (hitInfo.tHit == -1.0) {
        return evaluateEnvironment(rayDirection.xyz);
    }

    L += hitInfo.Le;

    const vec3 p = hitInfo.position;
    const vec3 n = hitInfo.normal;
    const vec3 sampleWeight = hitInfo.sampleWeight;
    const vec3 wi = -rayDirection.xyz;
    const uint materialId = hitInfo.materialId;
    const vec2 texCoord = hitInfo.texCoord;
    const bool deltaSample = hitInfo.sampleLobeType == kLobeTypeDelta;

    // BSDF sampling.
    {
        const float samplePdf = hitInfo.samplePdf;
        const vec3 sampleDirection = hitInfo.sampleDirection;
        traceRay(rng, kDimBounceBase + kDimsPerBounce, p, tMin, sampleDirection, tMax);

        if (hitInfo.lightId != -1) {
            const float lightPdf = getLightPdf(hitInfo.lightId, hitInfo.position - p, hitInfo.normal);
            const float misWeight = deltaSample ? 1.0f : powerHeuristic(samplePdf, lightPdf);
            L += sampleWeight * hitInfo.Le * misWeight;
        } else if (hitInfo.tHit < tMin && integrator.environmentEnabled != 0) {
            const float lightPdf = getEnvironmentLightPdf(sampleDirection);
            const float misWeight = deltaSample ? 1.0f : powerHeuristic(samplePdf, lightPdf);
            L += sampleWeight * evaluateEnvironment(sampleDirection) * misWeight;
        }
    }

    // Light sampling.
    vec3 shadowRayDir;
    float shadowRayLen;
    float lightPdf;
    bool lightIsDelta;
    setDimension(rng, kDimBounceBase + kDimLight);
    const vec3 radiance = sampleUniformLight(rng, p, shadowRayDir, shadowRayLen, lightPdf, lightIsDelta);
    if (lightPdf > 0.0f) {
        if (!traceShadowRay(p, 1e-5, shadowRayDir, shadowRayLen - 1e-5)) {
            const BsdfEval lightDirectionBsdf =
                evaluateBsdfWorldSpace(n, wi, shadowRayDir, materialId, texCoord);
            const float misWeight = lightIsDelta ? 1.0f : powerHeuristic(lightPdf, lightDirectionBsdf.pdf);
            L += radiance * lightDirectionBsdf.f * misWeight;
        }
    }

    return L;
}

vec3 computeRadianceMisPt(inout Sampler rng) {
    // Sample a point on the screen and transform it into a ray.
    const vec2 pixelSample = samplePixelPosition(rng);

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
        const uint bounceDim = kDimBounceBase + uint(bounceCount) * kDimsPerBounce;
        traceRay(rng, bounceDim, rayOrigin.xyz, tMin, rayDirection.xyz, tMax);
        if (hitInfo.tHit < tMin) {
            if (integrator.environmentEnabled != 0) {
                float misWeight = 1.0f;
                if (bounceCount > 0 && !prevWasDelta) {
                    misWeight = powerHeuristic(prevSamplePdf, getEnvironmentLightPdf(rayDirection.xyz));
                }
                L += throughput * evaluateEnvironment(rayDirection.xyz) * misWeight;
            }
            break;
        }

        if (hitInfo.lightId != -1) {
            float misWeight = 1.0f;
            if (bounceCount > 0 && !prevWasDelta) {
                const float lightPdf = getLightPdf(hitInfo.lightId, hitInfo.position - prevPosition, hitInfo.normal);
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
        const vec2 texCoord = hitInfo.texCoord;
        const bool isDelta = hitInfo.sampleLobeType == kLobeTypeDelta;

        // If the bounce wasn't a delta bounce (glass/mirror), do light sampling.
        if (!isDelta) {
            vec3 shadowRayDir;
            float shadowRayLen;
            float lightPdf;
            bool lightIsDelta;
            setDimension(rng, bounceDim + kDimLight);
            const vec3 radiance =
                sampleUniformLight(rng, p, shadowRayDir, shadowRayLen, lightPdf, lightIsDelta);
            if (lightPdf > 0.0f) {
                if (!traceShadowRay(p, 1e-5, shadowRayDir, shadowRayLen - 1e-5)) {
                    const BsdfEval lightDirectionBsdf =
                        evaluateBsdfWorldSpace(n, wi, shadowRayDir, materialId, texCoord);
                    const float misWeight =
                        lightIsDelta ? 1.0f : powerHeuristic(lightPdf, lightDirectionBsdf.pdf);
                    L += throughput * radiance * lightDirectionBsdf.f * misWeight;
                }
            }
        }

        if (dot(sampleWeight, sampleWeight) == 0.0f || dot(rayDir, rayDir) < 1e-12f) {
            break;
        }

        // Adjust throughput for the hit surface.
        throughput *= sampleWeight;

        prevPosition = p;
        prevSamplePdf = samplePdf;
        prevWasDelta = isDelta;

        // Setup the next ray.
        rayOrigin.xyz = p;
        rayDirection.xyz = rayDir;

        if (++bounceCount > kRussianRouletteCutoff) { // Cut the path tracing with Russian roulette.
            const float maxCoeff = max(throughput.x, max(throughput.y, throughput.z));
            const float q = 1.0f - min(maxCoeff, 0.99f);
            setDimension(rng, bounceDim + kDimRussianRoulette);
            if (next1D(rng) > q) {
                throughput /= 1.0f - q;
            } else {
                break;
            }
        }
    }

    return L;
}

vec3 computeRadiance(inout Sampler rng) {
    // Sample a point on the screen and transform it into a ray.
    const vec2 pixelSample = samplePixelPosition(rng);

    vec4 rayOrigin;
    vec4 rayDirection;
    sampleRay(rayOrigin, rayDirection, pixelSample);
    const float tMin = 1e-4;
    const float tMax = view.nearFar[1];

    // Accumulated radiance L for this path.
    vec3 L = vec3(0.0f);

    // Throughput of the current path, modulated by encountered BSDFs.
    vec3 throughput = vec3(1.0f);

    vec3 debugColor = vec3(0.0f);
    int bounceCount = 0;
    while (bounceCount < integrator.maxBounces) {
        const uint bounceDim = kDimBounceBase + uint(bounceCount) * kDimsPerBounce;
        traceRay(rng, bounceDim, rayOrigin.xyz, tMin, rayDirection.xyz, tMax);
        if (hitInfo.tHit >= tMin) {
            // Accumulate any emission from the hit surface (e.g. we hit a light).
            L += throughput * hitInfo.Le;

            if (dot(hitInfo.sampleWeight, hitInfo.sampleWeight) == 0.0f ||
                dot(hitInfo.sampleDirection, hitInfo.sampleDirection) < 1e-12f) {
                break;
            }

            // Adjust throughput for the hit surface.
            throughput *= hitInfo.sampleWeight; // equal to f(wi) * cos(wo) / pdf(wo).

            // Setup the next ray.
            rayOrigin.xyz = hitInfo.position;
            rayDirection.xyz = hitInfo.sampleDirection;
        } else { // The ray missed, evaluate environment lighting and exit the loop.
            L += throughput * evaluateEnvironment(rayDirection.xyz);
            break;
        }

        if (++bounceCount > kRussianRouletteCutoff) { // Cut the path tracing with Russian roulette.
            const float maxCoeff = max(throughput.x, max(throughput.y, throughput.z));
            const float q = 1.0f - min(maxCoeff, 0.99f);
            setDimension(rng, bounceDim + kDimRussianRoulette);
            if (next1D(rng) > q) {
                throughput /= 1.0f - q;
            } else {
                break;
            }
        }
    }

    return L;
}

void main() {
    vec3 L = vec3(0.0f);
    if (integrator.lightCount <= 0) {
        imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(L, 1.0));
        return;
    }

    // The sample index has to count accumulated samples, not frames, or every sample within a frame
    // would reuse the same point of the sequence.
    const uint sampleCount = uint(integrator.sampleCount);
    const uint baseSampleIdx = uint(integrator.sampleOffset);
    for (uint i = 0; i < sampleCount; ++i) {
        Sampler rng = createSampler(gl_LaunchIDEXT.xy, baseSampleIdx + i, integrator.seed);
        if (integrator.samplingMode == 0) {
            L += computeRadianceMisPt(rng);
        } else if (integrator.samplingMode == 1) {
            L += computeRadiance(rng);
        } else if (integrator.samplingMode == 2) {
            L += computeRadianceDirectLighting(rng);
        } else if (integrator.samplingMode == 3) {
            L += computeRadianceMis(rng);
        } else {
            L += computeRadianceMisPt(rng);
        }
    }
    L /= sampleCount;

    if (integrator.sampleOffset > 0) {
        const float t = float(integrator.sampleCount) / float(integrator.sampleOffset + integrator.sampleCount);
        const vec3 prevVal = imageLoad(image, ivec2(gl_LaunchIDEXT.xy)).xyz;
        L = mix(prevVal, L, t);
    }

    imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(L, 1.0));
}
