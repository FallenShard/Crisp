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
    mat4 LV;
    mat4 LP;
};

layout(location = 0) out float eyeZ;

void main()
{
    vec4 eyePos = LV * M * vec4(position, 1.0f);
    eyeZ = -eyePos.z;

    gl_Position = LP * eyePos;
}