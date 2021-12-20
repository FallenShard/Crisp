#version 460 core
#extension GL_EXT_ray_tracing : enable

struct hitPayload
{
  vec3 hitValue;
  float tHit;
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

// #define PI 3.1415926535897932384626433832795

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

//    imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(1, 0, 0, 1.0));
//    return;


    uint rayFlags = gl_RayFlagsOpaqueEXT;
    float tMin = 0.001;
    float tMax = 10000.0;

    traceRayEXT(topLevelAS, rayFlags, 0xFF, 0, 0, 0, origin.xyz, tMin, direction.xyz, tMax, 0);

    //vec3 center = vec3(0, 1, 0);
    //float radius = 1;
    vec3 testColor = vec3(1, 0.5, 0.5);

    if (prd.tHit >= 0.0f)
    {
        testColor = prd.hitValue;
    }
    else
    {
        testColor = vec3(0, 1, 0);
    }
    imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(testColor, 1.0));
    // testColor = direction.rgb;

    // testColor = vec3(prd.tHit);

    // if (prd.tHit >= 0.0f)
    // {
    //     // origin.xyz += prd.tHit * direction.xyz;
    //     // direction.xyz = normalize(vec3(1.0f, 1.0f, 0.0f));
    //     // traceNV(topLevelAS, rayFlags, 0xFF, 0, 0, 0, origin.xyz, tMin, direction.xyz, tMax, 0);

    //     if (prd.tHit >= 0.0f)
    //     {
    //         testColor = vec3(0);
    //         testColor = prd.hitValue;
    //     }
    //     else
    //     {
    //         testColor = vec3(1, 0.5, 0.5);
    //     }
    // }

    // if (prd.tHit >= 0.0f)
    // {
    //     origin.xyz += prd.tHit * direction.xyz;
    //     direction.xyz = normalize(vec3(-1.0f, -1.0f, 0.0f));

    //     const uint SAMPLE_COUNT = 1;
    //     float totalWeight = 0.0;   
    //     for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    //     {
    //         traceNV(topLevelAS, rayFlags, 0xFF, 0, 0, 0, origin.xyz, tMin, direction.xyz, tMax, 0);
    //         if (prd.tHit >= 0) {
    //             totalWeight += 1.0;
    //         }
    //     }

    //     testColor = vec3(1 - totalWeight / SAMPLE_COUNT);
    // }

    
}
