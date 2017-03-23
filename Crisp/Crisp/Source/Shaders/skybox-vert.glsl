#version 450 core

layout (location = 0) in vec3 position;

layout (location = 0) out vec3 texCoords;

layout(set = 0, binding = 0) uniform Transforms
{
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

void main()
{
    vec4 pos = MVP * vec4(position, 1.0f);
    gl_Position = pos.xyww;
    texCoords = position;
}