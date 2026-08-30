#version 460 core
#extension GL_EXT_descriptor_heap : require
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "PathTracer/Core/pbr-hit.part.glsl"

// Must match the heap slots in Scenes/PathTracedView.cpp.
const uint kEnvironmentMapSlot = 4;
const uint kEnvironmentSamplerSlot = 0;

layout(descriptor_heap, descriptor_stride = 64) uniform textureCube heapTextureCubes[];
layout(descriptor_heap, descriptor_stride = 64) uniform sampler heapSamplers[];

layout(location = 0) rayPayloadInEXT PbrHitInfo hitInfo;

void main() {
    hitInfo.tHit = -1.0f;
    hitInfo.emission = texture(
                           samplerCube(heapTextureCubes[kEnvironmentMapSlot], heapSamplers[kEnvironmentSamplerSlot]),
                           gl_WorldRayDirectionEXT)
                           .rgb;
}
