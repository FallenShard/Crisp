#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_descriptor_heap : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../Core/types.part.glsl"
#include "../../Common/math-constants.part.glsl"
#include "../Core/scene.part.glsl"

// Must match the heap slots in Scenes/VulkanRayTracingScene.cpp.
const uint kGgxAlbedoLutSlot = 5u;
const uint kGgxAlbedoLutSamplerSlot = 2u;

layout(descriptor_heap, descriptor_stride = 64) uniform texture2D heapTexture2Ds[];
layout(descriptor_heap, descriptor_stride = 64) uniform sampler heapSamplers[];

#define CRISP_GGX_ALBEDO_LUT sampler2D(heapTexture2Ds[kGgxAlbedoLutSlot], heapSamplers[kGgxAlbedoLutSamplerSlot])

#include "../../BSDFs/OpenPbr/surface.part.glsl"

layout(location = 0) callableDataInEXT BsdfSample bsdf;

// The first material type that is layered rather than a single lobe, which changes two things.
//
// Sampling reports the FULL mixture value and density, never just the lobe that was picked. The raygen weights
// next-event estimation against exactly this pdf, so reporting a single lobe's density would bias every MIS
// combination without producing a visibly wrong image.
//
// What does not hold yet: lobe selection still consumes unitSample.x and then rescales it for the direction,
// so the two are correlated. bsdf.lobeSample exists for exactly this and is deliberately left unused here --
// switching to it changes the image, and this callable is byte-for-byte what PathTracedView already renders.
// It lands with the real multi-lobe sampler; see docs/openpbr-path-tracer.md.
void main() {
    // Compensation is a per-view setting the callable cannot see; evaluateOpenPbr in bsdf-eval.part.glsl must
    // agree with this, or sampling and next-event estimation disagree.
    const OpenPbrSurface surface = createOpenPbrSurface(scene.materials.data[bsdf.materialId].surface,
                                                        kEnergyCompensationNone);

    bool sampledSpecular = false;
    const vec3 weight =
        sampleOpenPbrSurface(surface, bsdf.unitSample, bsdf.wi, bsdf.wo, bsdf.pdf, sampledSpecular);
    bsdf.f = weight * bsdf.pdf; // Recover f (BSDF * abs(cosThetaO)) from the sampled f / pdf weight.
    bsdf.lobeType = sampledSpecular ? kLobeTypeGlossy : kLobeTypeDiffuse;
}
