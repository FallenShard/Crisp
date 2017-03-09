#version 450 core

layout(location = 0) out vec4 finalColor;

layout(set = 1, binding = 0) uniform GuiColor
{
	vec4 values[16];
} guiColor;

layout(push_constant) uniform PushConstant
{
	layout(offset = 4) int value;
} colorIndex;

void main()
{
	finalColor = guiColor.values[colorIndex.value];
}