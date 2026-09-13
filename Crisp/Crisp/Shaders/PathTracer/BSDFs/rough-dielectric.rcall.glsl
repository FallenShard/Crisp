#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../Core/types.part.glsl"
#include "../Core/scene.part.glsl"
#include "../../BSDFs/rough-dielectric.part.glsl"

layout(location = 0) callableDataInEXT BsdfSample bsdf;

void main() {
    const BsdfParameters material = scene.materials.data[bsdf.materialId];
    bsdf.lobeType = kLobeTypeGlossy;
    if (bsdf.operation == kBsdfOperationSample) {
        sampleRoughDielectric(
            bsdf.unitSample,
            bsdf.lobeSample,
            kVacuumIor,
            material.surface.specularIor,
            material.microfacetType,
            material.microfacetAlpha,
            bsdf.wi,
            bsdf.wo,
            bsdf.f,
            bsdf.pdf);
        return;
    }

    bsdf.f = evaluateRoughDielectric(
        kVacuumIor,
        material.surface.specularIor,
        material.microfacetType,
        material.microfacetAlpha,
        bsdf.wi,
        bsdf.wo);
    bsdf.pdf = computeRoughDielectricPdf(
        kVacuumIor,
        material.surface.specularIor,
        material.microfacetType,
        material.microfacetAlpha,
        bsdf.wi,
        bsdf.wo);
}
