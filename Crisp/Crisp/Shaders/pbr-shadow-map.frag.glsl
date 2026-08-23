#version 460 core

layout(push_constant) uniform DrawParameters {
    uvec2 materialTableAddress;
    uint materialIndex;
    uint padding;
}
drawParameters;

void main() {
    // Keep the shadow variants pipeline-layout compatible while opaque casters avoid texture sampling.
    if (drawParameters.materialIndex == 0xffffffffu) {
        discard;
    }
}
