#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../Core/types.part.glsl"
#include "../../Common/math-constants.part.glsl"
#include "../Core/scene.part.glsl"
#include "../Textures/material-texture.part.glsl"
#include "../../BSDFs/lambertian.part.glsl"

layout(location = 0) callableDataInEXT BsdfSample bsdf;

void main() {
    if (bsdf.operation == kBsdfOperationSample) {
        bsdf.wo = sampleLambertian(bsdf.unitSample);
    }

    bsdf.lobeType = kLobeTypeDiffuse;
    const BsdfParameters material = scene.materials.data[bsdf.materialId];
    bsdf.f = evaluateLambertian(evaluateMaterialReflectance(material, bsdf.texCoord), bsdf.wi, bsdf.wo);
    bsdf.pdf = computeLambertianPdf(bsdf.wi, bsdf.wo);
}
