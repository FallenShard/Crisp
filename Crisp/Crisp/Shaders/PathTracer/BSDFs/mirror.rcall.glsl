#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../Core/types.part.glsl"
#include "../../BSDFs/mirror.part.glsl"

layout(location = 0) callableDataInEXT BsdfSample bsdf;

void main() {
    bsdf.lobeType = kLobeTypeDelta;
    sampleMirror(bsdf.wi, bsdf.wo, bsdf.f, bsdf.pdf);
}
