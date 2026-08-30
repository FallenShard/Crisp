#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_descriptor_heap : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "Common/math-constants.part.glsl"
#include "Common/rng.part.glsl"
#include "Common/view.part.glsl"
#include "PathTracer/Core/pbr-hit.part.glsl"

const int kRussianRouletteCutoff = 3;

// Must match the heap slots in Scenes/PathTracedView.cpp. The BVH is reached through a (set, binding) mapping
// rather than a subscript because acceleration structures cannot be heap arrays yet; see docs/descriptor-heap.md.
const uint kImageSlot = 1;
const uint kViewSlot = 2;
const uint kIntegratorSlot = 3;

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
    float environmentIntensity;
}
heapIntegrators[];

#define image heapStorageImages[kImageSlot]
#define view heapViews[kViewSlot].params
#define integrator heapIntegrators[kIntegratorSlot]

#include "PathTracer/Cameras/perspective.part.glsl"

layout(location = 0) rayPayloadEXT PbrHitInfo hitInfo;

// Fixed sampler layout, for the same reason as the analytic tracer's: every bounce restarts the cursor at its
// own base so a path that terminates early still consumes the same dimensions as one that does not.
const uint kDimPixelFilter = 0u;
const uint kDimBounceBase = 2u;
const uint kDimsPerBounce = 3u;
const uint kDimBsdf = 0u;            // 2 dimensions, relative to the bounce base.
const uint kDimRussianRoulette = 2u; // 1 dimension.

void traceBounce(inout Sampler rng, const uint bounceDim, const vec3 origin, const vec3 direction) {
    setDimension(rng, bounceDim + kDimBsdf);
    hitInfo.unitSample = next2D(rng);
    traceRayEXT(
        sceneBvh,
        gl_RayFlagsOpaqueEXT,
        0xFF,
        0,
        0,
        0,
        origin,
        1e-4f,
        direction,
        view.nearFar[1],
        /*payload=*/0);
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
    while (true) {
        const uint bounceDim = kDimBounceBase + uint(bounceCount) * kDimsPerBounce;
        traceBounce(rng, bounceDim, rayOrigin.xyz, rayDirection.xyz);

        if (hitInfo.tHit < 0.0f) { // Missed: the environment is the only light in this view.
            L += throughput * hitInfo.emission * integrator.environmentIntensity;
            break;
        }

        L += throughput * hitInfo.emission;

        if (bounceCount >= integrator.maxBounces || hitInfo.samplePdf <= 0.0f) {
            break;
        }

        throughput *= hitInfo.sampleWeight;

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
