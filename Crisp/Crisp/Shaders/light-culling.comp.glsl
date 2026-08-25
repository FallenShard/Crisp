#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "Common/view.part.glsl"

const uint kMaxLightsPerTile = 1024;

struct TileFrustum {
    vec4 planes[4];
};

layout(set = 0, binding = 0) readonly buffer TilePlanes {
    TileFrustum frusta[];
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

layout(set = 1, binding = 0, rg32ui) uniform writeonly uimage2D lightGrid;

layout(push_constant) uniform PushConstant {
    uint lightCount;
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

shared uint tileLightCount;
shared uint tileIndexOffset;
shared uint tileLightList[kMaxLightsPerTile];
shared TileFrustum tileFrustum;

uint getTileIndex() {
    return gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
}

bool isSphereOutsidePlane(const vec3 center, const float radius, const vec4 plane) {
    return dot(plane.xyz, center) - plane.w > radius;
}

bool isSphereInsideFrustum(const vec3 center, const float radius, in TileFrustum frustum) {
    for (int i = 0; i < 4; ++i) {
        if (isSphereOutsidePlane(center, radius, frustum.planes[i])) {
            return false;
        }
    }
    return true;
}

void main() {
    if (gl_LocalInvocationIndex == 0) {
        tileLightCount = 0;
        tileIndexOffset = 0;
        tileFrustum = frusta[getTileIndex()];
    }
    barrier();

    const uint threadCount = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z;
    for (uint i = gl_LocalInvocationIndex; i < lightCount; i += threadCount) {
        const vec3 eyeCenter = (view.V * vec4(lights[i].position.xyz, 1.0f)).xyz;
        if (isSphereInsideFrustum(eyeCenter, lights[i].params.r, tileFrustum)) {
            const uint slot = atomicAdd(tileLightCount, 1);
            if (slot < kMaxLightsPerTile) {
                tileLightList[slot] = i;
            }
        }
    }
    barrier();

    if (gl_LocalInvocationIndex == 0) {
        tileLightCount = min(tileLightCount, kMaxLightsPerTile);
        tileIndexOffset = atomicAdd(lightIndexCount, tileLightCount);
        imageStore(lightGrid, ivec2(gl_WorkGroupID.xy), uvec4(tileIndexOffset, tileLightCount, 0, 0));
    }
    barrier();

    for (uint i = gl_LocalInvocationIndex; i < tileLightCount; i += threadCount) {
        lightIndexList[tileIndexOffset + i] = tileLightList[i];
    }
}
