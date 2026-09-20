#version 460 core

layout(push_constant) uniform DrawParameters {
    uvec2 materialTableAddress;
    uint materialIndex;
    uint flags;
    uint transformIndex;
    uint padding;
}
drawParameters;

void main() {}
