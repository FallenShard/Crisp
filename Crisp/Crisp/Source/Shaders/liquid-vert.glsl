#version 450 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

out VsOut
{
	vec3 position;
	vec3 normal;
} vsOut;

layout(set = 0, binding = 0) uniform Transforms
{
	mat4 MVP;
	mat4 MV;
	mat4 M;
	mat4 N;
};

void main()
{
	vsOut.position = (MV * vec4(position, 1.0f)).xyz;
	vsOut.normal = (MV * vec4(normal, 0.0f)).xyz;
	
	gl_Position = MVP * vec4(position, 1.0f);
}