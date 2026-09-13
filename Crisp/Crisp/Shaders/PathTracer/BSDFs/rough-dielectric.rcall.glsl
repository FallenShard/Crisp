#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../Core/types.part.glsl"
#include "../Core/scene.part.glsl"
#include "../../BSDFs/rough-dielectric.part.glsl"

layout(location = 0) callableDataInEXT BrdfSample brdf;

void main() {
    const BrdfParameters material = scene.materials.data[brdf.materialId];
    brdf.lobeType = kLobeTypeGlossy;
    if (brdf.operation == kBrdfOperationSample) {
        sampleRoughDielectric(
            brdf.unitSample,
            brdf.lobeSample,
            material.exteriorIor,
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
        material.exteriorIor,
        material.surface.specularIor,
        material.microfacetType,
        material.microfacetAlpha,
        brdf.wi,
        brdf.wo);
    brdf.pdf = roughDielectricPdf(
        material.exteriorIor,
        material.surface.specularIor,
        material.microfacetType,
        material.microfacetAlpha,
        brdf.wi,
        brdf.wo);
}
