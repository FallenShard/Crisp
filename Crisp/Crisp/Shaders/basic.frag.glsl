#version 460 core

// Required even here: the mesh stage decorates its output PerPrimitiveEXT, and the interface only
// matches if the fragment input carries the same decoration. Without it the pipeline fails to
// create, which is what the extension being absent from a fragment shader usually looks like.
#extension GL_EXT_mesh_shader : require

perprimitiveEXT layout(location = 0) in flat vec3 inColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(inColor, 1.0);
}
