#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../Core/types.part.glsl"
#include "../Core/scene.part.glsl"
#include "../../BSDFs/smooth-dielectric.part.glsl"

layout(location = 0) callableDataInEXT BsdfSample bsdf;

void main() {
    bsdf.lobeType = kLobeTypeDelta;
    sampleSmoothDielectric(
        bsdf.unitSample,
        bsdf.wi,
        kVacuumIor,
        scene.materials.data[bsdf.materialId].surface.specularIor,
        bsdf.wo,
        bsdf.f,
        bsdf.pdf);
}
