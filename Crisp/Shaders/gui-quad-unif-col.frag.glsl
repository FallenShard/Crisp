#version 450 core

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 1) uniform GuiColor
{
	vec4 values[16];
} guiColor;

layout(push_constant) uniform PushConstant
{
	layout(offset = 4) int value;
} colorIndex;

void main()
{
	vec4 color = guiColor.values[colorIndex.value];
	// Premultiplied alpha
	finalColor = vec4(color.r, color.g, color.b, 1.0f) * color.a;// * color.a;
	// Standard alpha
	//finalColor = vec4(color.r, color.g, color.b, color.a);
}