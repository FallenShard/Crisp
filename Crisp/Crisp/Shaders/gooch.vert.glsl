#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

layout(location = 0) out vec3 eyePos;
layout(location = 1) out vec3 eyeNormal;
layout(location = 2) out vec3 worldPos;

layout(set = 0, binding = 0) uniform TransformPack
{
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

void main()
{
    worldPos    = vec4(M * vec4(position, 1.0f)).xyz;
    eyePos      = vec4(MV * vec4(position, 1.0f)).xyz;
    eyeNormal   = vec4(N * vec4(normal, 0.0f)).xyz;
    gl_Position = MVP * vec4(position, 1.0f);
}