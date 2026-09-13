#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../Core/types.part.glsl"
#include "../Core/scene.part.glsl"
#include "../../BSDFs/smooth-conductor.part.glsl"

layout(location = 0) callableDataInEXT BrdfSample brdf;

void main() {
    brdf.lobeType = kLobeTypeDelta;
    if (brdf.operation == kBrdfOperationEvaluate) {
        brdf.wo = vec3(0.0f);
        brdf.pdf = 0.0f;
        brdf.f = vec3(0.0f);
        return;
    }

    const BrdfParameters material = scene.materials.data[brdf.materialId];
    sampleSmoothConductor(
        brdf.normal,
        brdf.wi,
        material.complexIorEta,
        material.complexIorK,
        brdf.wo,
        brdf.f,
        brdf.pdf);
}
