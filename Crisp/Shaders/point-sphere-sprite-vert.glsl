#version 450 core

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 color;

layout(set = 0, binding = 0) uniform Transforms
{
	mat4 MVP;
	mat4 MV;
	mat4 M;
	mat4 N;
};

layout(set = 1, binding = 0) uniform ParticleParams
{
	float radius;
	float screenSpaceScale;
};

layout(location = 0) out vec3 outColor;

void main()
{
	vec3 eyePos   = (MV * position).xyz;
	outColor = color.xyz;

	gl_PointSize = radius * screenSpaceScale / -eyePos.z;
	gl_Position  = MVP * position;
}