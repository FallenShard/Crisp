#version 460 core

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require

#include "Common/bindless.part.glsl"

layout(location = 0) in vec2 inTexCoord;

#include "Common/openpbr-surface.part.glsl"

// The OpenPBR half is one nested block shared with the rasterizer, the other tracer and BrdfParameters;
// everything after it is a Crisp renderer extension. Must match PbrMaterialParams in Materials/PbrMaterial.hpp.
struct PbrMaterialParameters {
    OpenPbrSurfaceParams surface;

    vec2 uvScale;
    float normalScale;
    float aoStrength;

    uint samplerIndex;
    uint baseColorTex;
    uint normalTex;
    uint ormTex;

    uint emissionTex;
    float geometryOpacity;
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
        sampler2D(gTextures2D[material.baseColorTex], gSamplers[material.samplerIndex]), uv).a *
        material.geometryOpacity;
    if (alpha < material.alphaCutoff) {
        discard;
    }
}
