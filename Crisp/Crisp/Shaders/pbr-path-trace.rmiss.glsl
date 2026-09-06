#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_descriptor_heap : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Common/math-constants.part.glsl"
#include "PathTracer/Core/pbr-hit.part.glsl"

// Must match the heap slots in Scenes/PathTracedView.cpp.
const uint kEnvironmentEquirectSlot = 6;
const uint kEnvironmentSamplerSlot = 0;

layout(descriptor_heap, descriptor_stride = 64) uniform texture2D heapTexture2Ds[];
layout(descriptor_heap, descriptor_stride = 64) uniform sampler heapSamplers[];

#define CRISP_ENVIRONMENT_EQUIRECT     sampler2D(heapTexture2Ds[kEnvironmentEquirectSlot], heapSamplers[kEnvironmentSamplerSlot])
#include "PathTracer/Lights/pbr-environment.part.glsl"

layout(location = 0) rayPayloadInEXT PbrHitInfo hitInfo;

void main() {
    hitInfo.tHit = -1.0f;
    hitInfo.emission = evaluateEnvironmentRadiance(gl_WorldRayDirectionEXT);
}
