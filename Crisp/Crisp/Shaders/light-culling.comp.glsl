#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "Parts/view.part.glsl"

struct TileFrustum
{
    vec4 planes[4];
};

layout(set = 0, binding = 0) buffer TilePlanes
{
    TileFrustum frusta[];
};

layout (set = 0, binding = 1) buffer LightIndexCounter
{
    uint lightIndexCount[];
};


struct Light
{
    mat4 V;
    mat4 P;
    mat4 VP;
    vec4 position;
    vec4 direction;
    vec4 spectrum;
    vec4 params;
};

#define TotalLightCount 1024
layout(set = 0, binding = 2) buffer Lights
{
    Light lights[];
};

layout(set = 0, binding = 3) uniform View {
    ViewParameters view;
};

layout (set = 0, binding = 4) buffer LightIndexList
{
    uint lightIndexList[];
};

layout(set = 1, binding = 0, rg32ui) uniform writeonly uimage2D lightGrid;

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

uint getGridLinearIndex(uvec3 gridPosition, uvec3 gridDims)
{
    return gridPosition.z * gridDims.x * gridDims.y +
           gridPosition.y * gridDims.x +
           gridPosition.x;
}

uint getGlobalIndex()
{
    uvec3 dim = gl_WorkGroupSize * gl_NumWorkGroups;
    return gl_GlobalInvocationID.z * dim.x * dim.y +
           gl_GlobalInvocationID.y * dim.x +
           gl_GlobalInvocationID.x;
}

uint getGlobalGroupIndex()
{
    return gl_WorkGroupID.z * gl_NumWorkGroups.x * gl_NumWorkGroups.y +
           gl_WorkGroupID.y * gl_NumWorkGroups.x +
           gl_WorkGroupID.x;
}

//shared uint minDepth;
//shared uint maxDepth;

shared uint lightCount;
shared uint lightIndexStartOffset;
shared uint lightList[TotalLightCount];

shared TileFrustum groupFrustum;

void appendLight(uint lightId)
{
    uint index = atomicAdd(lightCount, 1);
    if (index < 1024)
        lightList[index] = lightId;
}

bool SphereInsidePlane(vec3 center, float radius, vec4 plane)
{
    return dot(plane.xyz, center) - plane.w < -radius;
}

bool SphereInsideFrustum(vec3 center, float radius, in TileFrustum frustum, float zNear, float zFar)
{
    bool result = true;
 
    // First check depth
    // Note: Here, the view vector points in the -Z axis so the 
    // far depth value will be approaching -infinity.
    if (center.z - radius > zNear || center.z + radius < zFar)
    {
        result = false;
    }
 
    // Then check frustum planes
    for ( int i = 0; i < 4 && result; i++ )
    {
        if (SphereInsidePlane(center, radius, frustum.planes[i]))
        {
            result = false;
        }
    }
 
    return result;
}

void main()
{
    uint threadIdx = getGlobalIndex();

    //if (gl_GlobalInvocationID.x >= 1920 || gl_GlobalInvocationID.y >= 1080)
    //  return;

    if (gl_LocalInvocationIndex == 0)
    {
      lightCount = 0;
      lightIndexStartOffset = 0;
      groupFrustum = frusta[getGlobalGroupIndex()];
    }

    memoryBarrierShared();
    barrier();

    float minDepth = -0.5f;
    float maxDepth = -1000.0f;

    uint localThreadCount = gl_WorkGroupSize.z * gl_WorkGroupSize.y * gl_WorkGroupSize.x;

    for (uint i = gl_LocalInvocationIndex; i < TotalLightCount; i += localThreadCount)
    {
       vec3 center = (view.V * vec4(lights[i].position.xyz, 1.0f)).xyz;
       float radius = lights[i].params.r;

       if (SphereInsideFrustum(center, radius, groupFrustum, minDepth, maxDepth))
           appendLight(i);
    }

    memoryBarrierShared();
    barrier();

    if (gl_LocalInvocationIndex == 0)
    {
       lightIndexStartOffset = atomicAdd(lightIndexCount[0], lightCount);
       uvec4 data = uvec4(lightIndexStartOffset, lightCount, 0, 0);
       imageStore(lightGrid, ivec2(gl_WorkGroupID.xy), data);
    }

    memoryBarrierShared();
    barrier();

    for (uint i = gl_LocalInvocationIndex; i < lightCount; i += localThreadCount)
        lightIndexList[lightIndexStartOffset + i] = lightList[i];
}