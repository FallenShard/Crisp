#version 460 core

layout(location = 0) in vec3 position;

struct TransformPack {
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

layout(std430, set = 1, binding = 0) readonly buffer Transforms {
    TransformPack transforms[];
};

layout(push_constant) uniform DrawParameters {
    uvec2 materialTableAddress;
    uint materialIndex;
    uint flags;
    uint transformIndex;
    uint padding;
}
drawParameters;

layout(set = 1, binding = 1) uniform Light {
    mat4 V;
    mat4 P;
    mat4 VP;
    vec4 position;
    vec4 direction;
    vec4 spectrum;
    vec4 params;
}
light;

void main() {
    gl_Position = light.VP * transforms[drawParameters.transformIndex].M * vec4(position, 1.0f);
}
