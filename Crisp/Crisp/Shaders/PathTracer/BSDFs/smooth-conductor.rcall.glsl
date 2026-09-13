#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../Core/types.part.glsl"
#include "../Core/scene.part.glsl"
#include "../../BSDFs/smooth-conductor.part.glsl"

layout(location = 0) callableDataInEXT BsdfSample bsdf;

void main() {
    bsdf.lobeType = kLobeTypeDelta;
    const BsdfParameters material = scene.materials.data[bsdf.materialId];
    sampleSmoothConductor(
        bsdf.wi,
        material.complexIorEta,
        material.complexIorK,
        bsdf.wo,
        bsdf.f,
        bsdf.pdf);
}
