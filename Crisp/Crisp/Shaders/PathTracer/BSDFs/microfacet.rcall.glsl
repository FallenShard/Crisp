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

layout(location = 0) callableDataInEXT BrdfSample brdf;

void main() {
    const BrdfParameters material = scene.materials.data[brdf.materialId];
    const float ks = material.surface.specularWeight;
    const float alpha = material.microfacetAlpha;
    const int microfacetType = material.microfacetType;

    if (brdf.operation == kBrdfOperationSample) {
        bool sampledSpecular;
        brdf.wo = sampleMicrofacet(brdf.unitSample, brdf.wi, ks, microfacetType, alpha, sampledSpecular);
        brdf.lobeType = sampledSpecular ? kLobeTypeGlossy : kLobeTypeDiffuse;
    } else {
        brdf.lobeType = kLobeTypeGlossy | kLobeTypeDiffuse;
    }

    brdf.f = evaluateMicrofacet(
        material.surface.baseColor,
        material.surface.specularWeight,
        material.exteriorIor,
        material.surface.specularIor,
        material.microfacetType,
        material.microfacetAlpha,
        brdf.wi,
        brdf.wo);
    brdf.pdf = microfacetPdf(
        brdf.wi, brdf.wo, material.surface.specularWeight, material.microfacetType, material.microfacetAlpha);
}
