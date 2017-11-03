#version 450 core

layout(location = 0) out vec4 finalColor;

void main()
{
	vec4 color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
	finalColor = vec4(color.r, color.g, color.b, 1.0f) * color.a;
}