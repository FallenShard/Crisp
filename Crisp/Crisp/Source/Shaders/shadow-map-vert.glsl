#version 450 core

layout(location = 0) in vec3 position;

layout(set = 0, binding = 0) uniform Transforms
{
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

layout(set = 0, binding = 1) uniform LightTransforms
{
    mat4 lightViewProjection;
    mat4 lightView;
    mat4 lightProjection;
};

void main()
{
    gl_Position = lightViewProjection * M * vec4(position, 1.0f);
}