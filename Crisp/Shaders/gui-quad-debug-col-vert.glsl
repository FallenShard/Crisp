#version 450 core

layout(location = 0) in vec2 position;

layout(push_constant) uniform PushConstant
{
	layout(offset = 0) mat4 value;
} transform;

void main()
{
	gl_Position = transform.value * vec4(position, 0.0f, 1.0f);
}