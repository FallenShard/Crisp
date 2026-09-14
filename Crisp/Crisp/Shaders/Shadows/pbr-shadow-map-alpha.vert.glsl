#version 460 core

layout(location = 0) in vec3 position;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec2 outTexCoord;

layout(set = 1, binding = 0) uniform Transforms {
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

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
    gl_Position = light.VP * M * vec4(position, 1.0f);
    outTexCoord = texCoord;
}
