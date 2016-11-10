#version 450 core

layout(location = 0) in vec2 position;

smooth out vec2 fsTexCoord;

void main()
{
	fsTexCoord = position * 0.5f + 0.5f;
	gl_Position = vec4(position, 0.0f, 1.0f);
}