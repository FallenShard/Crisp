#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../PathTracer/Core/types.part.glsl"
#include "../PathTracer/Core/scene.part.glsl"
#include "rough-conductor.part.glsl"

layout(location = 0) callableDataInEXT BrdfSample brdf;

void main() {
    const BrdfParameters material = scene.materials.data[brdf.materialId];
    if (brdf.operation == kBrdfOperationSample) {
        brdf.wo = sampleRoughConductor(
            brdf.unitSample, brdf.wi, material.microfacetType, material.microfacetAlpha);
    }

    brdf.lobeType = kLobeTypeGlossy;
    brdf.f = evaluateRoughConductor(
        material.complexIorEta,
        material.complexIorK,
        material.microfacetType,
        material.microfacetAlpha,
        brdf.wi,
        brdf.wo);
    brdf.pdf = roughConductorPdf(
        brdf.wi, brdf.wo, material.microfacetType, material.microfacetAlpha);
}
