#version 460 core

layout(location = 0) in vec3 position;

layout(location = 0) out vec3 localPosition;

layout(push_constant) uniform PushConstant
{
    layout(offset = 0) mat4 MVP;
};

void main()
{
    localPosition = position;
    gl_Position =  MVP * vec4(position, 1.0f);
}