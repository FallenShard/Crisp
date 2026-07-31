#version 450 core

#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 finalColor;

#include "Common/tonemap.part.glsl"

layout(set = 0, binding = 0) uniform TonemapParamsBlock {
    TonemapParams params;
};

layout(set = 1, binding = 0) uniform sampler2D hdrImage;

void main() {
    const vec4 hdr = textureLod(hdrImage, texCoord, 0);

    // Output stays linear; the sRGB encode is the last step in the chain, in GammaCorrect.frag.
    finalColor = vec4(applyTonemap(hdr.rgb, params), hdr.a);
}
