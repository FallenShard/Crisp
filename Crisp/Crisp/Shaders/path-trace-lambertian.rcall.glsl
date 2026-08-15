#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "Common/path-trace-payload.part.glsl"
#include "Common/math-constants.part.glsl"
#include "Common/path-trace-scene.part.glsl"
#include "Brdf/lambertian.part.glsl"

layout(location = 0) callableDataInEXT BrdfSample brdf;

void main() {
    if (brdf.operation == kBrdfOperationSample) {
        brdf.wo = sampleLambertian(brdf.unitSample);
    }

    brdf.lobeType = kLobeTypeDiffuse;
    brdf.f = evaluateLambertian(
        scene.materials.data[brdf.materialId].albedo, brdf.wi, brdf.wo);
    brdf.pdf = lambertianPdf(brdf.wi, brdf.wo);
}
