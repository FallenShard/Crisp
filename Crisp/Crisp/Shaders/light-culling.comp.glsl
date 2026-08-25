#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "Common/view.part.glsl"

// Must match kMaxLightsPerTile in Crisp/Lights/LightClustering.hpp.
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

layout(set = 1, binding = 1) uniform sampler2D depthTexture;

layout(push_constant) uniform PushConstant {
    uint lightCount;
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

shared uint tileLightCount;
shared uint tileIndexOffset;
shared uint tileLightList[kMaxLightsPerTile];
shared TileFrustum tileFrustum;

shared uint tileDepthMinBits;
shared uint tileDepthMaxBits;

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

float viewDepthFromReverseZ(const float depth, const float zNear) {
    return depth > 0.0f ? -zNear / depth : -1.0f / 0.0f;
}

bool isSphereInsideDepthRange(const vec3 center, const float radius, const float tileNearZ, const float tileFarZ) {
    return center.z - radius <= tileNearZ && center.z + radius >= tileFarZ;
}

void main() {
    if (gl_LocalInvocationIndex == 0) {
        tileLightCount = 0;
        tileIndexOffset = 0;
        tileDepthMinBits = 0xFFFFFFFFu;
        tileDepthMaxBits = 0u;
        tileFrustum = frusta[getTileIndex()];
    }
    barrier();

    const ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (all(lessThan(pixel, ivec2(view.screenSize)))) {
        const uint depthBits = floatBitsToUint(texelFetch(depthTexture, pixel, 0).r);
        atomicMin(tileDepthMinBits, depthBits);
        atomicMax(tileDepthMaxBits, depthBits);
    }
    barrier();

    if (tileDepthMaxBits == 0u) {
        if (gl_LocalInvocationIndex == 0) {
            imageStore(lightGrid, ivec2(gl_WorkGroupID.xy), uvec4(0, 0, 0, 0));
        }
        return;
    }

    const float zNear = view.nearFar.x;
    const float tileNearZ = viewDepthFromReverseZ(uintBitsToFloat(tileDepthMaxBits), zNear);
    const float tileFarZ = viewDepthFromReverseZ(uintBitsToFloat(tileDepthMinBits), zNear);

    const uint threadCount = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z;
    for (uint i = gl_LocalInvocationIndex; i < lightCount; i += threadCount) {
        const vec3 eyeCenter = (view.V * vec4(lights[i].position.xyz, 1.0f)).xyz;
        const float radius = lights[i].params.r;
        if (!isSphereInsideDepthRange(eyeCenter, radius, tileNearZ, tileFarZ)) {
            continue;
        }
        if (!isSphereInsideFrustum(eyeCenter, radius, tileFrustum)) {
            continue;
        }

        const uint slot = atomicAdd(tileLightCount, 1);
        if (slot < kMaxLightsPerTile) {
            tileLightList[slot] = i;
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
