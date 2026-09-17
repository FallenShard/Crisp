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

layout(descriptor_heap, descriptor_stride = 64, scalar) uniform IntegratorParams {
    int maxBounces;
    int sampleCount;
    int sampleOffset;
    uint seed;

    int reconstructionFilter;
    int lightCount;
    int samplingMode;
    int environmentEnabled;

    int environmentWidth;
    int environmentHeight;
    float environmentIntensity;
    uint visibilityMask;

    vec3 mediumAbsorption;
    vec3 mediumScattering;
    float mediumAnisotropy;
    int mediumType;
    vec3 mediumBoundsMin;
    vec3 mediumBoundsMax;
    float mediumMaximumDensity;
} heapIntegrators[];

#define integrator heapIntegrators[kIntegratorSlot]

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

#define image heapStorageImages[kImageSlot]
#define view heapViews[kViewSlot].params
#define environmentMap heapTexture2Ds[kEnvironmentSlot]
#define environmentSampler heapSamplers[kEnvironmentSamplerSlot]
layout(descriptor_heap, descriptor_stride = 64) uniform texture3D heapTexture3Ds[];
#define mediumVolume sampler3D(heapTexture3Ds[kMediumVolumeSlot], heapSamplers[kMediumSamplerSlot])
// bsdf-eval.part.glsl evaluates kBsdfOpenPbr inline for next-event estimation, and that reaches the GGX
// directional-albedo table even when no compensation mode is active.
#define CRISP_GGX_ALBEDO_LUT sampler2D(heapTexture2Ds[kGgxAlbedoLutSlot], heapSamplers[kGgxAlbedoLutSamplerSlot])
#define CRISP_MATERIAL_TEXTURE(heapIndex) sampler2D(heapTexture2Ds[heapIndex], heapSamplers[kMaterialSamplerSlot])

#include "Core/scene-addresses.part.glsl"
#include "Core/intersection.part.glsl"
#include "Textures/material-texture.part.glsl"
#include "Textures/pbr-material-texture.part.glsl"
#include "BSDFs/bsdf-eval.part.glsl"
#include "Media/homogeneous.part.glsl"
#include "Media/heterogeneous.part.glsl"
#include "Media/volume-bounds.part.glsl"
#include "PhaseFunctions/henyey-greenstein.part.glsl"

vec3 computeMediumShadowTransmittance(
    const Sampler pathSampler, const uint bounceDim, const vec3 origin, const vec3 direction,
    const float distance, const vec3 extinction) {
    float entry;
    float exitDistance;
    if (!intersectMediumBounds(
            origin, direction, distance, integrator.mediumBoundsMin, integrator.mediumBoundsMax,
            entry, exitDistance)) {
        return vec3(1.0f);
    }
    const float mediumDistance = exitDistance - entry;
    if (integrator.mediumType == 0) {
        return homogeneousMediumTransmittance(extinction, mediumDistance);
    }
    Sampler trackingSampler = createMediumTrackingSampler(pathSampler, bounceDim, 0x5ad039e5u);
    return heterogeneousMediumTransmittance(
        trackingSampler, origin + entry * direction, direction, mediumDistance, extinction,
        integrator.mediumMaximumDensity, integrator.mediumBoundsMin, integrator.mediumBoundsMax);
}

BsdfEval evaluateBsdfWorldSpace(
    vec3 normal, vec3 wi, vec3 wo, uint materialId, uint materialTextureOffset, vec2 texCoord) {
    PbrMaterialParameters material = scene.materials.data[materialId];
    applyMaterialTextures(material, materialTextureOffset, texCoord);

    const mat3 coordinateFrame = createCoordinateFrame(normal);
    const mat3 worldToLocal = transpose(coordinateFrame);
    return evaluateBsdf(material, texCoord, worldToLocal * wi, worldToLocal * wo);
}

void traceRay(
    inout Sampler rng, in uint bounceDimBase, in vec3 rayOrigin, in float tMin, in vec3 rayDirection, in float tMax) {
    setDimension(rng, bounceDimBase + kDimBsdf);
    hitInfo.bsdfSample = next2D(rng);
    hitInfo.bsdfLobeSample = next1D(rng);
    traceRayEXT(
        sceneBvh,
        gl_RayFlagsOpaqueEXT,
        integrator.visibilityMask,
        0,
        0,
        0,
        rayOrigin,
        tMin,
        rayDirection,
        tMax,
        kPayloadIndex);
}

bool traceShadowRay(in vec3 rayOrigin, in float tMin, in vec3 rayDirection, in float tMax) {
    rayQueryEXT rayQuery;
    rayQueryInitializeEXT(
        rayQuery,
        sceneBvh,
        gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT,
        integrator.visibilityMask,
        rayOrigin,
        tMin,
        rayDirection,
        tMax);
    rayQueryProceedEXT(rayQuery);
    return rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT;
}

#include "Cameras/perspective.part.glsl"
#include "Lights/light-sampling.part.glsl"

float balanceHeuristic(const float pdfA, const float pdfB) {
    const float total = pdfA + pdfB;
    return total > 0.0f ? pdfA / total : 0.0f;
}

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
        return evaluateEnvironmentLight(rayDirection.xyz);
    }

    L += hitInfo.Le;

    const vec3 p = hitInfo.position;
    const vec3 n = hitInfo.normal;
    const vec3 wi = -rayDirection.xyz;
    const uint materialId = hitInfo.materialId;
    const uint materialTextureOffset = hitInfo.materialTextureOffset;
    const vec2 texCoord = hitInfo.texCoord;

    setDimension(rng, kDimBounceBase + kDimLight);
    const LightSample lightSample = sampleUniformLight(rng, p);
    if (lightSample.pdf > 0.0f) {
        if (!traceShadowRay(p, 1e-5, lightSample.direction, lightSample.distance - 1e-5)) {
            const BsdfEval lightDirectionBsdf = evaluateBsdfWorldSpace(n, wi, lightSample.direction, materialId, materialTextureOffset, texCoord);
            L += lightSample.weight * lightDirectionBsdf.f;
        }
    }

    return L;
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
        return evaluateEnvironmentLight(rayDirection.xyz);
    }

    L += hitInfo.Le;

    const vec3 p = hitInfo.position;
    const vec3 n = hitInfo.normal;
    const vec3 sampleWeight = hitInfo.sampleWeight;
    const vec3 wi = -rayDirection.xyz;
    const uint materialId = hitInfo.materialId;
    const uint materialTextureOffset = hitInfo.materialTextureOffset;
    const vec2 texCoord = hitInfo.texCoord;
    const bool deltaSample = hitInfo.sampleLobeType == kLobeTypeDelta;

    // BSDF sampling.
    {
        const float samplePdf = hitInfo.samplePdf;
        const vec3 sampleDirection = hitInfo.sampleDirection;
        traceRay(rng, kDimBounceBase + kDimsPerBounce, p, tMin, sampleDirection, tMax);

        if (hitInfo.lightId != -1) {
            const float lightPdf = computeLightPdf(
                uint(hitInfo.lightId),
                normalize(hitInfo.position - p),
                hitInfo.position - p,
                hitInfo.normal);
            const float misWeight = deltaSample ? 1.0f : balanceHeuristic(samplePdf, lightPdf);
            L += sampleWeight * hitInfo.Le * misWeight;
        } else if (hitInfo.tHit < tMin && integrator.environmentEnabled != 0) {
            const float lightPdf = computeLightPdf(environmentLightIndex(), sampleDirection, vec3(0.0f), vec3(0.0f));
            const float misWeight = deltaSample ? 1.0f : balanceHeuristic(samplePdf, lightPdf);
            L += sampleWeight * evaluateEnvironmentLight(sampleDirection) * misWeight;
        }
    }

    // Light sampling.
    setDimension(rng, kDimBounceBase + kDimLight);
    const LightSample lightSample = sampleUniformLight(rng, p);
    if (lightSample.pdf > 0.0f) {
        if (!traceShadowRay(p, 1e-5, lightSample.direction, lightSample.distance - 1e-5)) {
            const BsdfEval lightDirectionBsdf =
                evaluateBsdfWorldSpace(n, wi, lightSample.direction, materialId, materialTextureOffset, texCoord);
            const float misWeight = lightSample.isDelta ? 1.0f : balanceHeuristic(lightSample.pdf, lightDirectionBsdf.pdf);
            L += lightSample.weight * lightDirectionBsdf.f * misWeight;
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
                    misWeight = balanceHeuristic(prevSamplePdf, computeLightPdf(environmentLightIndex(), rayDirection.xyz, vec3(0.0f), vec3(0.0f)));
                }
                L += throughput * evaluateEnvironmentLight(rayDirection.xyz) * misWeight;
            }
            break;
        }

        if (hitInfo.lightId != -1) {
            float misWeight = 1.0f;
            if (bounceCount > 0 && !prevWasDelta) {
                const float lightPdf = computeLightPdf(
                    uint(hitInfo.lightId),
                    normalize(hitInfo.position - prevPosition),
                    hitInfo.position - prevPosition,
                    hitInfo.normal);
                misWeight = balanceHeuristic(prevSamplePdf, lightPdf);
            }
            L += throughput * hitInfo.Le * misWeight;
        } else {
            L += throughput * hitInfo.Le;
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
        const uint materialTextureOffset = hitInfo.materialTextureOffset;
        const vec2 texCoord = hitInfo.texCoord;
        const bool isDelta = hitInfo.sampleLobeType == kLobeTypeDelta;

        // If the bounce wasn't a delta bounce (glass/mirror), do light sampling.
        if (!isDelta) {
            setDimension(rng, bounceDim + kDimLight);
            const LightSample lightSample = sampleUniformLight(rng, p);
            if (lightSample.pdf > 0.0f) {
                if (!traceShadowRay(p, 1e-5, lightSample.direction, lightSample.distance - 1e-5)) {
                    const BsdfEval lightDirectionBsdf =
                        evaluateBsdfWorldSpace(n, wi, lightSample.direction, materialId, materialTextureOffset, texCoord);
                    const float misWeight =
                        lightSample.isDelta ? 1.0f : balanceHeuristic(lightSample.pdf, lightDirectionBsdf.pdf);
                    L += throughput * lightSample.weight * lightDirectionBsdf.f * misWeight;
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

vec3 computeRadianceVolumeMisPt(inout Sampler rng) {
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

        const bool surfaceHit = hitInfo.tHit >= tMin;
        const float segmentDistance = surfaceHit ? hitInfo.tHit : tMax;
        const vec3 extinction = integrator.mediumAbsorption + integrator.mediumScattering;
        float mediumDistance = segmentDistance;
        bool mediumEvent = false;
        vec3 mediumWeight = vec3(1.0f);
        float mediumEntry;
        float mediumExit;
        if (intersectMediumBounds(
                rayOrigin.xyz, rayDirection.xyz, segmentDistance,
                integrator.mediumBoundsMin, integrator.mediumBoundsMax, mediumEntry, mediumExit)) {
            const float boundedDistance = mediumExit - mediumEntry;
            if (integrator.mediumType == 0) {
                setDimension(rng, bounceDim + kDimMediumDistance);
                const float localDistance = sampleHomogeneousMediumDistance(next2D(rng), extinction);
                mediumEvent = localDistance < boundedDistance;
                const vec3 transmittance = homogeneousMediumTransmittance(
                    extinction, mediumEvent ? localDistance : boundedDistance);
                const float distancePdf = homogeneousMediumDistancePdf(extinction, transmittance, mediumEvent);
                mediumWeight = mediumEvent
                    ? transmittance * integrator.mediumScattering / max(distancePdf, 1e-30f)
                    : transmittance / max(distancePdf, 1e-30f);
                if (mediumEvent) {
                    mediumDistance = mediumEntry + localDistance;
                }
            } else {
                Sampler trackingSampler = createMediumTrackingSampler(rng, bounceDim + kDimMediumDistance, 0x9e3779b9u);
                const float localDistance = sampleHeterogeneousMediumDistance(
                    trackingSampler, rayOrigin.xyz + mediumEntry * rayDirection.xyz, rayDirection.xyz,
                    boundedDistance, extinction, integrator.mediumScattering, integrator.mediumMaximumDensity,
                    integrator.mediumBoundsMin, integrator.mediumBoundsMax, mediumWeight);
                mediumEvent = localDistance < boundedDistance;
                if (mediumEvent) {
                    mediumDistance = mediumEntry + localDistance;
                }
            }
        }
        throughput *= mediumWeight;

        if (!mediumEvent && !surfaceHit) {
            if (integrator.environmentEnabled != 0) {
                float misWeight = 1.0f;
                if (bounceCount > 0 && !prevWasDelta) {
                    misWeight = balanceHeuristic(
                        prevSamplePdf,
                        computeLightPdf(environmentLightIndex(), rayDirection.xyz, vec3(0.0f), vec3(0.0f)));
                }
                L += throughput * evaluateEnvironmentLight(rayDirection.xyz) * misWeight;
            }
            break;
        }

        if (!mediumEvent && hitInfo.lightId != -1) {
            float misWeight = 1.0f;
            if (bounceCount > 0 && !prevWasDelta) {
                const float lightPdf = computeLightPdf(
                    uint(hitInfo.lightId),
                    normalize(hitInfo.position - prevPosition),
                    hitInfo.position - prevPosition,
                    hitInfo.normal);
                misWeight = balanceHeuristic(prevSamplePdf, lightPdf);
            }
            L += throughput * hitInfo.Le * misWeight;
        } else if (!mediumEvent) {
            L += throughput * hitInfo.Le;
        }

        if (bounceCount >= integrator.maxBounces) {
            break;
        }

        vec3 p;
        vec3 sampleWeight;
        float samplePdf;
        vec3 rayDir;
        bool isDelta;

        if (mediumEvent) {
            p = rayOrigin.xyz + mediumDistance * rayDirection.xyz;
            sampleWeight = vec3(1.0f);
            isDelta = false;

            setDimension(rng, bounceDim + kDimPhase);
            rayDir = sampleHenyeyGreensteinPhase(next2D(rng), rayDirection.xyz, integrator.mediumAnisotropy);
            samplePdf = computeHenyeyGreensteinPhasePdf(rayDirection.xyz, rayDir, integrator.mediumAnisotropy);

            // The RGB scattering / distance-PDF weight is already in throughput. Attenuate the shadow segment.
            setDimension(rng, bounceDim + kDimLight);
            const LightSample lightSample = sampleUniformLight(rng, p);
            if (lightSample.pdf > 0.0f) {
                if (!traceShadowRay(p, 1e-5, lightSample.direction, lightSample.distance - 1e-5)) {
                    const float phase = evaluateHenyeyGreensteinPhase(
                        rayDirection.xyz, lightSample.direction, integrator.mediumAnisotropy);
                    const float misWeight = lightSample.isDelta ? 1.0f : balanceHeuristic(lightSample.pdf, phase);
                    const vec3 shadowTransmittance = computeMediumShadowTransmittance(
                        rng, bounceDim, p, lightSample.direction, min(lightSample.distance, tMax), extinction);
                    L += throughput * lightSample.weight * phase * shadowTransmittance * misWeight;
                }
            }
        } else {
            p = hitInfo.position;
            sampleWeight = hitInfo.sampleWeight;
            samplePdf = hitInfo.samplePdf;
            rayDir = hitInfo.sampleDirection;
            isDelta = hitInfo.sampleLobeType == kLobeTypeDelta;

            // If the bounce wasn't a delta bounce (glass/mirror), do light sampling.
            if (!isDelta) {
                const vec3 n = hitInfo.normal;
                const vec3 wi = -rayDirection.xyz;
                const uint materialId = hitInfo.materialId;
                const uint materialTextureOffset = hitInfo.materialTextureOffset;
                const vec2 texCoord = hitInfo.texCoord;

                setDimension(rng, bounceDim + kDimLight);
                const LightSample lightSample = sampleUniformLight(rng, p);
                if (lightSample.pdf > 0.0f) {
                    if (!traceShadowRay(p, 1e-5, lightSample.direction, lightSample.distance - 1e-5)) {
                        const BsdfEval lightDirectionBsdf = evaluateBsdfWorldSpace(
                            n, wi, lightSample.direction, materialId, materialTextureOffset, texCoord);
                        const float misWeight =
                            lightSample.isDelta ? 1.0f : balanceHeuristic(lightSample.pdf, lightDirectionBsdf.pdf);
                        const vec3 shadowTransmittance = computeMediumShadowTransmittance(
                            rng, bounceDim, p, lightSample.direction, min(lightSample.distance, tMax), extinction);
                        L += throughput * lightSample.weight * lightDirectionBsdf.f * shadowTransmittance * misWeight;
                    }
                }
            }
        }

        if (dot(sampleWeight, sampleWeight) == 0.0f || dot(rayDir, rayDir) < 1e-12f) {
            break;
        }

        // Adjust throughput for the sampled surface or medium event.
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
            L += throughput * evaluateEnvironmentLight(rayDirection.xyz);
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
        } else if (integrator.samplingMode == 4) {
            L += computeRadianceVolumeMisPt(rng);
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
