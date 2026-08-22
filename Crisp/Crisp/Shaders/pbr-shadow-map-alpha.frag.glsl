#version 450 core

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require

#include "Common/bindless.part.glsl"

layout(location = 0) in vec2 inTexCoord;

struct PbrMaterialParameters {
    vec4 albedo;
    vec3 emissiveFactor;
    float normalScale;
    vec2 uvScale;
    float metallic;
    float roughness;
    float aoStrength;

    uint samplerIndex;
    uint albedoTex;
    uint normalTex;
    uint ormTex;
    uint emissiveTex;
    float alphaCutoff;
    uint flags;
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer PbrMaterialTable {
    PbrMaterialParameters materials[];
};

layout(push_constant) uniform DrawParameters {
    PbrMaterialTable materialTable;
    uint materialIndex;
    uint padding;
}
drawParameters;

void main() {
    const PbrMaterialParameters material = drawParameters.materialTable.materials[drawParameters.materialIndex];
    const vec2 uv = inTexCoord * material.uvScale;
    const float alpha = texture(
        sampler2D(gTextures2D[material.albedoTex], gSamplers[material.samplerIndex]), uv).a * material.albedo.a;
    if (alpha < material.alphaCutoff) {
        discard;
    }
}
