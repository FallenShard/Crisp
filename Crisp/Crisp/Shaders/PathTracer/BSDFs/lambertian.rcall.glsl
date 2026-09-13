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

layout(location = 0) callableDataInEXT BrdfSample brdf;

void main() {
    if (brdf.operation == kBrdfOperationSample) {
        brdf.wo = sampleLambertian(brdf.unitSample);
    }

    brdf.lobeType = kLobeTypeDiffuse;
    const BrdfParameters material = scene.materials.data[brdf.materialId];
    brdf.f = evaluateLambertian(evaluateMaterialReflectance(material, brdf.texCoord), brdf.wi, brdf.wo);
    brdf.pdf = lambertianPdf(brdf.wi, brdf.wo);
}
