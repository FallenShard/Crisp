#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../Core/types.part.glsl"
#include "../../Common/math-constants.part.glsl"
#include "../../Common/warp.part.glsl"
#include "../Core/scene.part.glsl"
#include "../../BSDFs/microfacet.part.glsl"

layout(location = 0) callableDataInEXT BsdfSample bsdf;

void main() {
    const BsdfParameters material = scene.materials.data[bsdf.materialId];
    const float ks = material.surface.specularWeight;
    const float alpha = material.microfacetAlpha;
    const int microfacetType = material.microfacetType;

    bool sampledSpecular;
    bsdf.wo = sampleMicrofacet(bsdf.unitSample, bsdf.wi, ks, microfacetType, alpha, sampledSpecular);
    bsdf.lobeType = sampledSpecular ? kLobeTypeGlossy : kLobeTypeDiffuse;

    bsdf.f = evaluateMicrofacet(
        material.surface.baseColor,
        material.surface.specularWeight,
        kVacuumIor,
        material.surface.specularIor,
        material.microfacetType,
        material.microfacetAlpha,
        bsdf.wi,
        bsdf.wo);
    bsdf.pdf = computeMicrofacetPdf(
        bsdf.wi, bsdf.wo, material.surface.specularWeight, material.microfacetType, material.microfacetAlpha);
}
