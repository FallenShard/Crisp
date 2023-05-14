#version 460 core
#extension GL_EXT_ray_tracing : require

#define PI 3.1415926535897932384626433832795
#define InvPI 0.31830988618379067154
const int kMaxBounces = 1024;
const int kPayloadIndex = 0;

struct HitInfo
{
    vec3 position;
    float tHit;

    vec3 sampleDirection;
    float samplePdf;

    vec3 Le;
    uint bounceCount;

    vec3 bsdfEval;
    uint rngSeed;

    vec4 debugValue;
};

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

// From https://redirect.cs.umbc.edu/~olano/papers/GPUTEA.pdf.
uint tea(uint val0, uint val1)
{
  uint v0 = val0;
  uint v1 = val1;
  uint s0 = 0;

  for(uint n = 0; n < 16; n++)
  {
    s0 += 0x9e3779b9;
    v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
    v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
  }

  return v0;
}

// uint xorshift32(inout uint state)
// {
// 	/* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
// 	uint x = state;
// 	x ^= x << 13;
// 	x ^= x >> 17;
// 	x ^= x << 5;
//     state = x;
// 	return x;
// }

// float rndFloat(inout uint state)
// {
//     uint s = xorshift32(state);
//     return uintBitsToFloat(s >> 9) - 1.0f;
// }

uint rndUint(inout uint seed)
{
    return (seed = 1664525 * seed + 1013904223);
}

float rndFloat(inout uint seed)
{
    const uint one = 0x3f800000;
    const uint msk = 0x007fffff;
    return uintBitsToFloat(one | (msk & (rndUint(seed) >> 9))) - 1;
}

layout(push_constant) uniform PushConstant
{
    uint frameIdx;
} pushConst;

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
    while (bounceCount < kMaxBounces)
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
    uint seed = tea(gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x, pushConst.frameIdx);

    vec3 L = vec3(0.0f);
    const uint sampleCount = 10;
    for (uint i = 0; i < sampleCount; ++i)
    {
        L += computeRadiance(seed);
    }
    L /= sampleCount;

    if (pushConst.frameIdx > 0)
    {
       const float t = 1.0f / (pushConst.frameIdx + 1);
       const vec3 oldVal = imageLoad(image, ivec2(gl_LaunchIDEXT.xy)).xyz;
       imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(mix(oldVal, L, t), 1.0));
    }
    else
    {
       imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(L, 1.0));
    }
}
