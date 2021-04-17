#version 460 core

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstant
{
    layout(offset = 192) vec4 color;
};

void main()
{
    outColor = color;
}