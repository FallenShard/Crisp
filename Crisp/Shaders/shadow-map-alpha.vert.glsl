#version 450 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec2 outTexCoord;

layout(set = 0, binding = 0) uniform Transforms
{
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

layout(set = 0, binding = 1) uniform Light
{
    mat4 V;
    mat4 P;
    mat4 VP;
    vec4 position;
    vec4 direction;
    vec4 spectrum;
    vec4 params;
} light;

void main()
{
    outTexCoord = texCoord;
    gl_Position = light.VP * M * vec4(position, 1.0f);
}