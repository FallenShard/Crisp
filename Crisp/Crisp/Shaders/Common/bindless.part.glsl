#ifndef BINDLESS_PART_GLSL
#define BINDLESS_PART_GLSL

// The global bindless table. Must match BindlessImageRegistry: the set index, the binding numbers, and the fact
// that several view types share binding 0 because they are all VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE.
//
// The including shader must request GL_EXT_nonuniform_qualifier itself: an #extension directive is only valid
// before any non-preprocessor token, which an include cannot guarantee.
//
// BINDLESS_SET is 3 while the tree still owns sets 0-2. The end state is set 0; see docs/renderer-roadmap.md.
#define BINDLESS_SET 3

layout(set = BINDLESS_SET, binding = 0) uniform texture2D gTextures2D[];
layout(set = BINDLESS_SET, binding = 2) uniform sampler gSamplers[];

// nonuniformEXT on both indices because this is the general entry point. Where the index is provably wave
// uniform - read from a UBO bound per draw, as in the PBR material - it costs an AMD waterfall loop for
// nothing, and the caller is better off indexing gTextures2D directly.
vec4 sampleBindless(const uint textureIndex, const uint samplerIndex, const vec2 uv) {
    return texture(sampler2D(gTextures2D[nonuniformEXT(textureIndex)], gSamplers[nonuniformEXT(samplerIndex)]), uv);
}

#endif
