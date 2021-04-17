#version 450 core

layout(location = 0) out vec4 finalColor;

layout(push_constant) uniform Color
{
    layout(offset = 0) vec4 value;
} color;

void main()
{
	finalColor = color.value;
}