#version 450 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec3 bitangent;

layout(location = 0) out vec3 eyeNormal;
layout(location = 1) out vec3 eyePosition;
layout(location = 2) out vec3 worldPos;

layout(set = 0, binding = 0) uniform Transforms
{
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

void main()
{
    eyeNormal    = normalize((N * vec4(normal, 0.0f)).xyz);
    eyePosition  = (MV * vec4(position, 1.0f)).xyz;
    worldPos     = (M * vec4(position, 1.0f)).xyz;
    
    gl_Position = MVP * vec4(position, 1.0f);
}