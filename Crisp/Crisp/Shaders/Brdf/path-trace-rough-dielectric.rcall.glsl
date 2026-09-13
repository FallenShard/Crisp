#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../PathTracer/Core/types.part.glsl"
#include "../PathTracer/Core/scene.part.glsl"
#include "rough-dielectric.part.glsl"

layout(location = 0) callableDataInEXT BrdfSample brdf;

void main() {
    const BrdfParameters material = scene.materials.data[brdf.materialId];
    brdf.lobeType = kLobeTypeGlossy;
    if (brdf.operation == kBrdfOperationSample) {
        sampleRoughDielectric(
            brdf.unitSample,
            brdf.lobeSample,
            material.extIor,
            material.surface.specularIor,
            material.microfacetType,
            material.microfacetAlpha,
            brdf.wi,
            brdf.wo,
            brdf.f,
            brdf.pdf);
        return;
    }

    brdf.f = evaluateRoughDielectric(
        material.extIor,
        material.surface.specularIor,
        material.microfacetType,
        material.microfacetAlpha,
        brdf.wi,
        brdf.wo);
    brdf.pdf = roughDielectricPdf(
        material.extIor,
        material.surface.specularIor,
        material.microfacetType,
        material.microfacetAlpha,
        brdf.wi,
        brdf.wo);
}
