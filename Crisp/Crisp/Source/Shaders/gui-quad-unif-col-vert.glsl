#version 450 core

layout(location = 0) in vec2 position;

layout(set = 0, binding = 0) uniform GuiTransform
{
	mat4 MVP[4];
} guiTransform;

layout(push_constant) uniform PushConstant
{
	layout(offset = 0) int index;
} pushConsts;

void main()
{
	gl_Position = guiTransform.MVP[pushConsts.index] * vec4(position, 0.0f, 1.0f);
}