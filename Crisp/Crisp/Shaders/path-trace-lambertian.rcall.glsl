#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "Common/path-trace-payload.part.glsl"
#include "Common/math-constants.part.glsl"
#include "Common/warp.part.glsl"
#include "Common/path-trace-scene.part.glsl"

layout(location = 0) callableDataInEXT BrdfSample brdf;

void main() {
    if (brdf.operation == kBrdfOperationSample) {
        brdf.wo = squareToCosineHemisphere(brdf.unitSample);
    }

    brdf.lobeType = kLobeTypeDiffuse;

    if (brdf.wi.z <= 0.0f || brdf.wo.z <= 0.0f) {
        brdf.pdf = 0.0f;
        brdf.f = vec3(0.0f);
        return;
    }

    brdf.pdf = squareToCosineHemispherePdf(brdf.wo);
    brdf.f = scene.materials.data[brdf.materialId].albedo * InvPI * brdf.wo.z;
}
