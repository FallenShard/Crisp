#ifndef CRISP_PATH_TRACER_MATERIAL_TEXTURE_GLSL
#define CRISP_PATH_TRACER_MATERIAL_TEXTURE_GLSL

#include "../../Common/pbr-material.part.glsl"

#extension GL_EXT_descriptor_heap : require
#extension GL_EXT_nonuniform_qualifier : require

layout(descriptor_heap, descriptor_stride = 64) uniform texture2D heapTexture2Ds[];
layout(descriptor_heap, descriptor_stride = 64) uniform sampler heapSamplers[];

vec3 evaluateMaterialReflectance(const PbrMaterialParameters material, const vec2 texCoord) {
    if (material.baseColorTex == 0u) {
        return material.surface.baseColor;
    }

    const uint textureIndex = nonuniformEXT(material.baseColorTex);
    const uint samplerIndex = nonuniformEXT(material.samplerIndex);
    return texture(sampler2D(heapTexture2Ds[textureIndex], heapSamplers[samplerIndex]), texCoord).rgb;
}

#endif // CRISP_PATH_TRACER_MATERIAL_TEXTURE_GLSL
