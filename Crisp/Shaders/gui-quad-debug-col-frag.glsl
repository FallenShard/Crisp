#version 450 core

layout(location = 0) out vec4 finalColor;

layout(push_constant) uniform PushConstant
{
    layout(offset = 64) vec4 value;
} colorVec;

void main()
{
    vec4 color = colorVec.value;
    finalColor = vec4(color.r, color.g, color.b, 1.0f) * color.a;
}