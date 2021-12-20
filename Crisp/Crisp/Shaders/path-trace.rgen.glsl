#version 460
#extension GL_EXT_ray_tracing : enable

struct hitPayload
{
    vec3 hitPos;
    float tHit;
    vec3 sampleDir;
    float pdf;
    vec3 Le;
    uint bounceCount;
    vec3 bsdf;
    float pad2;
    vec4 debugValue;
};

layout(location = 0) rayPayloadEXT hitPayload prd;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 1, rgba32f) uniform image2D image;

layout(set = 0, binding = 2) uniform Camera
{
    mat4 V;
    mat4 P;
    vec2 screenSize;
    vec2 nearFar;
} camera;

// bool analyticIntersection(const vec3 center, float radius, const vec3 rorg, const vec3 rdir, float tnear, float tfar, out float t)
// {
//     vec3 centerToOrigin = rorg - center;

//     float b = 2.0f * dot(rdir, centerToOrigin);
//     float c = dot(centerToOrigin, centerToOrigin) - radius * radius;

//     float discrim = b * b - 4.0f * c;

//     if (discrim < 0.0f) // both complex solutions, no intersection
//     {
//         return false;
//     }
//     else if (discrim < 1e-5) // Discriminant is practically zero
//     {
//         t = -0.5f * b;
//     }
//     else
//     {
//         float factor = (b > 0.0f) ? -0.5f * (b + sqrt(discrim)) : -0.5f * (b - sqrt(discrim));
//         float t0 = factor;
//         float t1 = c / factor;

//         if (t0 > t1) // Prefer the smaller t, select larger if smaller is less than tNear
//         {
//             t = t1 < tnear ? t0 : t1;
//         }
//         else
//         {
//             t = t0 < tnear ? t1 : t0;
//         }
//     }

//     if (t < tnear || t > tfar)
//         return false;
//     else
//         return true;
// }

#define PI 3.1415926535897932384626433832795
#define InvPI 0.31830988618379067154

// vec3 squareToUniformCylinder(const vec2 pt, float cosThetaMin, float cosThetaMax)
// {
//     float z = cosThetaMin + pt.y * (cosThetaMax - cosThetaMin);
//     float phi = 2.0f * PI * pt.x;

//     return vec3(cos(phi), sin(phi), z);
// }

// vec3 cylinderToSphereSection(const vec2 pt, float cosThetaMin, float cosThetaMax)
// {
//     vec3 cylinderPt = squareToUniformCylinder(pt, cosThetaMin, cosThetaMax);
//     float radius = sqrt(1.0f - cylinderPt.z * cylinderPt.z);
//     return vec3(cylinderPt.x * radius, cylinderPt.y * radius, cylinderPt.z);
// }

// vec3 squareToUniformSphere(const vec2 pt)
// {
//     return cylinderToSphereSection(pt, 1.0f, -1.0f);
// }

// float RadicalInverse_VdC(uint bits) 
// {
//     bits = (bits << 16u) | (bits >> 16u);
//     bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
//     bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
//     bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
//     bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
//     return float(bits) * 2.3283064365386963e-10; // / 0x100000000
// }

// vec2 Hammersley(uint i, uint N)
// {
//     return vec2(float(i)/float(N), RadicalInverse_VdC(i));
// } 

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

// Generate a random unsigned int in [0, 2^24) given the previous RNG state
// using the Numerical Recipes linear congruential generator
uint lcg(inout uint prev)
{
  uint LCG_A = 1664525u;
  uint LCG_C = 1013904223u;
  prev       = (LCG_A * prev + LCG_C);
  return prev & 0x00FFFFFF;
}

// Generate a random float in [0, 1) given the previous RNG state
float rnd(inout uint prev)
{
  return (float(lcg(prev)) / float(0x01000000));
}

uint RandomInt(inout uint seed)
{
    // LCG values from Numerical Recipes
    return (seed = 1664525 * seed + 1013904223);
}

float RandomFloat(inout uint seed)
{
    // Float version using bitmask from Numerical Recipes
    const uint one = 0x3f800000;
    const uint msk = 0x007fffff;
    return uintBitsToFloat(one | (msk & (RandomInt(seed) >> 9))) - 1;
}

vec3 cosineToHemisphere(vec2 randomSample)
{
    float radius = sqrt(randomSample.y);
    float theta = 2.0f * PI * randomSample.x;
    float x = radius * cos(theta);
    float y = radius * sin(theta);
    float z = sqrt(max(0.0f, 1.0f - x * x - y * y));
    return vec3(x, y, z);
}

layout(push_constant) uniform PushConstant
{
    uint frameIdx;
} pushConst;

void main() 
{
    const vec2 pixelCenter = vec2(ivec2(gl_LaunchIDEXT.xy)) + vec2(0.5f);
    const vec2 inUV = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
    vec2 d = inUV * 2.0 - 1.0;
    mat4 invV = inverse(camera.V);
    mat4 invP = inverse(camera.P);
    vec4 origin = invV * vec4(0, 0, 0, 1);
    vec4 target = invP * vec4(d, 1, 1);
    vec4 direction = invV * vec4(normalize(target.xyz), 0.0f);


    uint rayFlags = gl_RayFlagsOpaqueEXT;
    float tMin = 0.001;
    float tMax = 10000.0;

    // Accumulated radiance L for this path
    vec3 L = vec3(0.0f);

    // Throughput of the current path
    vec3 throughput = vec3(1.0f);

    uint seed = tea(gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x, pushConst.frameIdx);
    vec3 debugColor = vec3(0.0f);
    int maxBounces = 1024;
    int bounceCount = 0;
    while (bounceCount < maxBounces)
    {
        prd.bounceCount = bounceCount;
        traceRayEXT(topLevelAS, rayFlags, 0xFF, 0, 0, 0, origin.xyz, tMin, direction.xyz, tMax, 0);
        if (prd.tHit >= tMin)
        {
            // Accumulate if we hit the light
            L += throughput * prd.Le;

            // Adjust throughput for the hit surface
            throughput *= prd.bsdf; // equal to f(wi) * cos(wo) / pdf(wo)

            // Setup the next ray
            origin.xyz    = prd.hitPos;
            direction.xyz = prd.sampleDir;

            debugColor = prd.debugValue.xyz;
        }
        else // miss
        {
            break;
        }

        if (bounceCount == 0)
            debugColor.r = prd.debugValue.x;
        else
            debugColor.g = prd.debugValue.x;

        bounceCount++;

        if (bounceCount > 10)
        {
            float maxCoeff = max(throughput.x, max(throughput.y, throughput.z));
            float q = 1.0f - min(maxCoeff, 0.99f);

            if (RandomFloat(seed) > q)
                throughput /= (1.0f - q);
            else
                break;
        }
    }

    if (pushConst.frameIdx > 0)
    {
       float t = 1.0f / (pushConst.frameIdx + 1);
       vec3 oldVal = imageLoad(image, ivec2(gl_LaunchIDEXT.xy)).xyz;
       imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(mix(oldVal, L, t), 1.0));
    }
    else
    {
       imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(L, 1.0));
    }

//    if (prd.tHit > 0)
//    {
//        imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(prd.Le, 1.0));
//    }
//    else
//    {
//        imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(1, 0, 0, 1.0));
//    }
//    

    //imageStore(image, ivec2(gl_LaunchIDNV.xy), vec4(debugColor, 1.0));
}
