#version 450 core

in VsOut
{
	vec3 position;
} fsIn;

layout(location = 0) out vec4 finalColor;

layout(set = 1, binding = 0) uniform Color
{
	vec4 colors[16];
};

layout(push_constant) uniform PushConstant
{
	layout(offset = 0) int value;
} colorIndex;

void main()
{
	finalColor = vec4(2 * fsIn.position, 1.0f);
}