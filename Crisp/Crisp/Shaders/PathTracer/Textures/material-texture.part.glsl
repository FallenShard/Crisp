#ifndef CRISP_PATH_TRACER_MATERIAL_TEXTURE_GLSL
#define CRISP_PATH_TRACER_MATERIAL_TEXTURE_GLSL

#extension GL_EXT_descriptor_heap : require
#extension GL_EXT_nonuniform_qualifier : require

layout(descriptor_heap, descriptor_stride = 64) uniform texture2D heapTexture2Ds[];
layout(descriptor_heap, descriptor_stride = 64) uniform sampler heapSamplers[];

vec3 evaluateMaterialReflectance(const BrdfParameters material, const vec2 texCoord) {
    if (material.reflectanceTexture < 0 || material.reflectanceSampler < 0) {
        return material.albedo;
    }

    const int textureIndex = nonuniformEXT(material.reflectanceTexture);
    const int samplerIndex = nonuniformEXT(material.reflectanceSampler);
    return texture(sampler2D(heapTexture2Ds[textureIndex], heapSamplers[samplerIndex]), texCoord).rgb;
}

#endif // CRISP_PATH_TRACER_MATERIAL_TEXTURE_GLSL
