#version 450 core

layout(location = 0) in vec2 position;

uniform mat4 MVP;
uniform float z = 0.0f;

void main()
{
	gl_Position = MVP * vec4(position, z, 1.0f);
}