#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../Core/types.part.glsl"
#include "../../BSDFs/mirror.part.glsl"

layout(location = 0) callableDataInEXT BsdfSample bsdf;

void main() {
    bsdf.lobeType = kLobeTypeDelta;
    if (bsdf.operation == kBsdfOperationEvaluate) {
        bsdf.wo = vec3(0.0f);
        bsdf.pdf = 0.0f;
        bsdf.f = vec3(0.0f);
        return;
    }

    sampleMirror(bsdf.wi, bsdf.wo, bsdf.f, bsdf.pdf);
}
