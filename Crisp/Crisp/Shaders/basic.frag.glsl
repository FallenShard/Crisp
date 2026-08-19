#version 460 core

#extension GL_EXT_mesh_shader : require

perprimitiveEXT layout(location = 0) in flat vec3 inColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(inColor, 1.0);
}
