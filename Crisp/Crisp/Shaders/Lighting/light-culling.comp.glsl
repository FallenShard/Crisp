#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "../Common/view.part.glsl"

// Must match kMaxLightsPerCluster in Crisp/Lights/LightClustering.hpp.
const uint kMaxLightsPerCluster = 256;

struct ClusterAabb {
    vec4 minPoint;
    vec4 maxPoint;
};

layout(set = 0, binding = 0) readonly buffer ClusterAabbs {
    ClusterAabb clusters[];
};

layout(set = 0, binding = 1) buffer LightIndexCounter {
    uint lightIndexCount;
};

struct LightDescriptor {
    mat4 V;
    mat4 P;
    mat4 VP;
    vec4 position;
    vec4 direction;
    vec4 spectrum;
    vec4 params;
};

layout(set = 0, binding = 2) readonly buffer Lights {
    LightDescriptor lights[];
};

layout(set = 0, binding = 3) uniform View {
    ViewParameters view;
};

layout(set = 0, binding = 4) writeonly buffer LightIndexList {
    uint lightIndexList[];
};

layout(set = 0, binding = 5) writeonly buffer LightGrid {
    uvec2 lightGrid[];
};

layout(push_constant) uniform PushConstant {
    uint lightCount;
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

shared uint clusterLightCount;
shared uint clusterIndexOffset;
shared uint clusterLightList[kMaxLightsPerCluster];
shared ClusterAabb clusterBounds;

uint getClusterIndex() {
    return (gl_WorkGroupID.z * gl_NumWorkGroups.y + gl_WorkGroupID.y) * gl_NumWorkGroups.x + gl_WorkGroupID.x;
}

bool isSphereInsideCluster(const vec3 center, const float radius, in ClusterAabb aabb) {
    const vec3 closest = clamp(center, aabb.minPoint.xyz, aabb.maxPoint.xyz);
    const vec3 delta = center - closest;
    return dot(delta, delta) <= radius * radius;
}

void main() {
    const uint clusterIndex = getClusterIndex();

    if (gl_LocalInvocationIndex == 0) {
        clusterLightCount = 0;
        clusterIndexOffset = 0;
        clusterBounds = clusters[clusterIndex];
    }
    barrier();

    const uint threadCount = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z;
    for (uint i = gl_LocalInvocationIndex; i < lightCount; i += threadCount) {
        const vec3 eyeCenter = (view.V * vec4(lights[i].position.xyz, 1.0f)).xyz;
        if (!isSphereInsideCluster(eyeCenter, lights[i].params.r, clusterBounds)) {
            continue;
        }

        const uint slot = atomicAdd(clusterLightCount, 1);
        if (slot < kMaxLightsPerCluster) {
            clusterLightList[slot] = i;
        }
    }
    barrier();

    if (gl_LocalInvocationIndex == 0) {
        clusterLightCount = min(clusterLightCount, kMaxLightsPerCluster);
        clusterIndexOffset = atomicAdd(lightIndexCount, clusterLightCount);
        lightGrid[clusterIndex] = uvec2(clusterIndexOffset, clusterLightCount);
    }
    barrier();

    for (uint i = gl_LocalInvocationIndex; i < clusterLightCount; i += threadCount) {
        lightIndexList[clusterIndexOffset + i] = clusterLightList[i];
    }
}
