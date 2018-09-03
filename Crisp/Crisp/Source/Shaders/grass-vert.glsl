#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 col0;
layout(location = 2) in vec4 col1;
layout(location = 3) in vec4 col2;
layout(location = 4) in vec4 col3;

layout(set = 0, binding = 0) uniform Camera
{
    mat4 V;
    mat4 P;
    vec2 screenSize;
    vec2 nearFar;
};

layout(location = 0) out vec3 worldPos;
layout(location = 1) out vec3 eyePos;

void main()
{
    mat4 M   = mat4(col0, col1, col2, col3);
    mat4 MV  = V * M;
    mat4 MVP = P * MV;

    worldPos  = (M * vec4(position, 1.0f)).xyz;
    eyePos    = (MV * vec4(position, 1.0f)).xyz;
    gl_Position  = MVP * vec4(position, 1.0f);
}