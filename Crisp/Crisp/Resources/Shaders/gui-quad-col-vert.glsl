#version 450 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;

smooth out vec3 fsColor;

uniform mat4 MVP;
uniform float z = 0.0f;

void main()
{
	fsColor = color;
	gl_Position = MVP * vec4(position, z, 1.0f);
}