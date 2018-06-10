#version 450 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 tangent;

layout(location = 0) out vec3 eyePos;
layout(location = 1) out vec3 eyeNormal;
layout(location = 2) out vec3 worldPos;
layout(location = 3) out vec2 outTexCoord;
layout(location = 4) out vec3 eyeTangent;

layout(set = 0, binding = 0) uniform Transforms
{
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

void main()
{
    outTexCoord  = texCoord;
    worldPos     = (M  * vec4(position, 1.0f)).xyz;
    eyePos       = (MV * vec4(position, 1.0f)).xyz;
    eyeNormal    = (MV * vec4(normal, 0.0f)).xyz;
    eyeTangent   = (MV * vec4(tangent, 0.0f)).xyz;
    gl_Position  = MVP * vec4(position, 1.0f);
}