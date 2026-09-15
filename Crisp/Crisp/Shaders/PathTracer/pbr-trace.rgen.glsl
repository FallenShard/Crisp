#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_descriptor_heap : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_ray_query : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../Common/math-constants.part.glsl"
#include "../Common/rng.part.glsl"
#include "../Common/view.part.glsl"
#include "../Common/warp.part.glsl"
#include "Core/heap-slots.part.glsl"
#include "Core/hit-info.part.glsl"

const int kRussianRouletteCutoff = 3;

layout(set = 1, binding = 0) uniform accelerationStructureEXT sceneBvh;
layout(descriptor_heap, descriptor_stride = 64, rgba32f) uniform image2D heapStorageImages[];

layout(descriptor_heap, descriptor_stride = 64) uniform View {
    ViewParameters params;
} heapViews[];

layout(descriptor_heap, descriptor_stride = 64) uniform IntegratorParams {
    int maxBounces;
    int sampleCount;
    int frameIdx;
    float environmentIntensity;
    uint visibilityMask;
    int environmentWidth;
    int environmentHeight;
} heapIntegrators[];

#define image heapStorageImages[kImageSlot]
#define view heapViews[kViewSlot].params
#define integrator heapIntegrators[kIntegratorSlot]

layout(descriptor_heap, descriptor_stride = 64) uniform texture2D heapTexture2Ds[];
layout(descriptor_heap, descriptor_stride = 64) uniform sampler heapSamplers[];

#define CRISP_ENVIRONMENT_EQUIRECT     sampler2D(heapTexture2Ds[kEnvironmentSlot], heapSamplers[kEnvironmentSamplerSlot])
#define CRISP_GGX_ALBEDO_LUT sampler2D(heapTexture2Ds[kGgxAlbedoLutSlot], heapSamplers[kGgxAlbedoLutSamplerSlot])
#define CRISP_MATERIAL_TEXTURE(heapIndex) sampler2D(heapTexture2Ds[heapIndex], heapSamplers[kMaterialSamplerSlot])

#include "Core/scene-addresses.part.glsl"
#include "BSDFs/pbr-surface.part.glsl"
#include "Textures/pbr-material-texture.part.glsl"
#include "Lights/pbr-environment.part.glsl"
#include "Cameras/perspective.part.glsl"

layout(location = 0) rayPayloadEXT HitInfo hitInfo;

// Fixed sampler layout, for the same reason as the analytic tracer's: every bounce restarts the cursor at its
// own base so a path that terminates early still consumes the same dimensions as one that does not.
const uint kDimPixelFilter = 0u;
const uint kDimBounceBase = 2u;
const uint kDimsPerBounce = 6u;
const uint kDimBsdf = 0u;            // 3 dimensions, relative to the bounce base.
const uint kDimLight = 3u;           // 2 dimensions.
const uint kDimRussianRoulette = 5u; // 1 dimension.

void traceBounce(inout Sampler rng, const uint bounceDim, const vec3 origin, const vec3 direction) {
    setDimension(rng, bounceDim + kDimBsdf);
    hitInfo.bsdfSample = next2D(rng);
    hitInfo.bsdfLobeSample = next1D(rng);
    traceRayEXT(
        sceneBvh,
        gl_RayFlagsOpaqueEXT,
        integrator.visibilityMask,
        0,
        0,
        0,
        origin,
        1e-4f,
        direction,
        view.nearFar[1],
        /*payload=*/0);
}

bool isOccluded(const vec3 origin, const vec3 direction, const float tMax) {
    rayQueryEXT query;
    rayQueryInitializeEXT(
        query,
        sceneBvh,
        gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT,
        integrator.visibilityMask,
        origin,
        1e-4f,
        direction,
        tMax);
    while (rayQueryProceedEXT(query)) {}
    return rayQueryGetIntersectionTypeEXT(query, true) != gl_RayQueryCommittedIntersectionNoneEXT;
}

// Next-event estimation against the environment. Sampling the CDF puts samples where the radiance is, which is
// what a small bright sun needs; the BSDF lobe alone would find it only by chance. The balance heuristic then
// blends this with the BSDF-sampled hit below so neither strategy double-counts.
//
// This rebuilds the surface the hit shader sampled, rather than being handed it. The payload would have to
// carry a resolved lobe, which only works while every material is one PbrSurface; re-deriving it from the
// material record is what lets an arbitrary BSDF answer the same query later.
vec3 estimateEnvironmentDirect(const vec3 wiWorld, const vec2 lightSample) {
    const uint width = uint(integrator.environmentWidth);
    const uint height = uint(integrator.environmentHeight);
    if (width == 0u || height == 0u) {
        return vec3(0.0f);
    }

    float lightPdf;
    const vec3 direction =
        sampleEnvironmentLightDirection(scene.environmentCdf, width, height, lightSample, lightPdf);
    if (lightPdf <= 0.0f) {
        return vec3(0.0f);
    }

    const mat3 frame = createCoordinateFrame(hitInfo.normal);
    const vec3 wo = transpose(frame) * direction;
    if (wo.z <= 0.0f) {
        return vec3(0.0f);
    }

    PbrMaterialParameters material = scene.materials.data[hitInfo.materialId];
    applyMaterialTextures(material, hitInfo.materialTextureOffset, hitInfo.texCoord);
    const PbrSurface surface = createPbrSurface(material, scene.energyCompensation);

    const vec3 wi = transpose(frame) * wiWorld;
    const vec3 f = evaluatePbrSurface(surface, wi, wo); // Already carries the cosine.
    if (f == vec3(0.0f)) {
        return vec3(0.0f);
    }

    if (isOccluded(hitInfo.position, direction, 1e27f)) {
        return vec3(0.0f);
    }

    const float bsdfPdf = computePbrSurfacePdf(surface, wi, wo);
    const vec3 radiance = evaluateEnvironmentRadiance(direction) * integrator.environmentIntensity;
    return f * radiance * environmentMisWeight(lightPdf, bsdfPdf) / lightPdf;
}

vec3 computeRadiance(inout Sampler rng) {
    setDimension(rng, kDimPixelFilter);
    const vec2 pixelSample = vec2(gl_LaunchIDEXT.xy) + next2D(rng);

    vec4 rayOrigin;
    vec4 rayDirection;
    sampleRay(rayOrigin, rayDirection, pixelSample);

    vec3 L = vec3(0.0f);
    vec3 throughput = vec3(1.0f);

    int bounceCount = 0;
    // Carried across the loop so a BSDF-sampled ray that lands on the environment can be weighted against the
    // density next-event estimation would have used for the same direction.
    float previousBsdfPdf = 0.0f;
    // Only the camera ray is not a BSDF sample. Every lobe in PbrSurface has a finite density -- alpha is
    // clamped away from zero, so there is no delta lobe -- which means every later bounce must be MIS-weighted
    // or its environment hit is counted twice, once here and once by next-event estimation.
    bool cameFromCamera = true;

    while (true) {
        const uint bounceDim = kDimBounceBase + uint(bounceCount) * kDimsPerBounce;
        traceBounce(rng, bounceDim, rayOrigin.xyz, rayDirection.xyz);

        if (hitInfo.tHit < 0.0f) { // Missed: the environment is the only light in this view.
            const vec3 radiance = evaluateEnvironmentRadiance(rayDirection.xyz) * integrator.environmentIntensity;
            // The camera ray takes the full contribution; everything else splits with the light sampler.
            float weight = 1.0f;
            if (!cameFromCamera) {
                const uint width = uint(integrator.environmentWidth);
                const uint height = uint(integrator.environmentHeight);
                const float lightPdf = width == 0u || height == 0u
                    ? 0.0f
                    : environmentDirectionPdf(scene.environmentCdf, width, height, rayDirection.xyz);
                weight = environmentMisWeight(previousBsdfPdf, lightPdf);
            }
            L += throughput * radiance * weight;
            break;
        }

        L += throughput * hitInfo.Le;

        setDimension(rng, bounceDim + kDimLight);
        L += throughput * estimateEnvironmentDirect(-rayDirection.xyz, next2D(rng));

        if (bounceCount >= integrator.maxBounces || hitInfo.samplePdf <= 0.0f) {
            break;
        }

        throughput *= hitInfo.sampleWeight;
        previousBsdfPdf = hitInfo.samplePdf;
        cameFromCamera = false;

        rayOrigin.xyz = hitInfo.position;
        rayDirection.xyz = hitInfo.sampleDirection;

        if (++bounceCount > kRussianRouletteCutoff) {
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
    // The sample index counts accumulated samples, not frames, or every sample within a frame would reuse the
    // same point of the sequence.
    const uint sampleCount = uint(max(integrator.sampleCount, 1));
    const uint baseSampleIdx = uint(integrator.frameIdx) * sampleCount;

    vec3 L = vec3(0.0f);
    for (uint i = 0; i < sampleCount; ++i) {
        Sampler rng = createSampler(gl_LaunchIDEXT.xy, baseSampleIdx + i);
        L += computeRadiance(rng);
    }
    L /= float(sampleCount);

    if (integrator.frameIdx > 0) {
        const float t = 1.0f / float(integrator.frameIdx + 1);
        const vec3 previous = imageLoad(image, ivec2(gl_LaunchIDEXT.xy)).xyz;
        L = mix(previous, L, t);
    }

    imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(L, 1.0f));
}
