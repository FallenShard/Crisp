#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Parts/path-tracer-payload.part.glsl"
#include "Parts/math-constants.part.glsl"
#include "Parts/rng.part.glsl"

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
} integrator;

void sampleRay(out vec4 origin, out vec4 direction, in vec2 pixelSample)
{
    const vec2 ndcSample = pixelSample / vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0; // In [-1, 1].

    origin = camera.invV * vec4(0, 0, 0, 1);

    const vec4 target = camera.invP * vec4(ndcSample, 0, 1);
    const vec3 rayDirEyeSpace = normalize(target.xyz);

    direction = camera.invV * vec4(rayDirEyeSpace, 0.0f);
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
        L += computeRadiance(seed);
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
