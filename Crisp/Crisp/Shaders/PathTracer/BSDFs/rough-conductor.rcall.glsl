#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../Core/types.part.glsl"
#include "../Core/scene.part.glsl"
#include "../../BSDFs/rough-conductor.part.glsl"

layout(location = 0) callableDataInEXT BsdfSample bsdf;

void main() {
    const BsdfParameters material = scene.materials.data[bsdf.materialId];
    bsdf.wo = sampleRoughConductor(
        bsdf.unitSample, bsdf.wi, material.microfacetType, material.microfacetAlpha);
    bsdf.lobeType = kLobeTypeGlossy;
    bsdf.f = evaluateRoughConductor(
        material.complexIorEta,
        material.complexIorK,
        material.microfacetType,
        material.microfacetAlpha,
        bsdf.wi,
        bsdf.wo);
    bsdf.pdf = computeRoughConductorPdf(
        bsdf.wi, bsdf.wo, material.microfacetType, material.microfacetAlpha);
}
