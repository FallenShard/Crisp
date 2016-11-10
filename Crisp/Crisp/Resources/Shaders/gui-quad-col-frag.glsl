#version 450 core

smooth in vec3 fsColor;

out vec4 finalColor;

uniform vec4 offset;

void main()
{
	finalColor = vec4(fsColor, 1.0f) + offset;
}