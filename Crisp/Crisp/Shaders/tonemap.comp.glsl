#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "Common/tonemap.part.glsl"

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(set = 0, binding = 0) uniform TonemapParamsBlock {
    TonemapParams params;
};

layout(set = 1, binding = 0) uniform sampler2D hdrImage;
layout(set = 1, binding = 1, rgba16f) uniform writeonly image2D tonemappedImage;

void main() {
    const ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
    const ivec2 extent = imageSize(tonemappedImage);
    if (texel.x >= extent.x || texel.y >= extent.y) {
        return;
    }

    const vec4 hdr = texelFetch(hdrImage, texel, 0);

    // Output stays linear; the sRGB encode is the last step in the chain, in GammaCorrect.frag.
    imageStore(tonemappedImage, texel, vec4(applyTonemap(hdr.rgb, params), hdr.a));
}
