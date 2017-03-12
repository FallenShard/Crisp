#version 450 core

layout(location = 0) in vec3 position;

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

out VsOut
{
	vec3 eyePos;
} vsOut;

void main()
{
	vsOut.eyePos = (MV * vec4(position, 1.0f)).xyz;

	gl_PointSize = 2.0f;//radius * screenSpaceScale / -vsOut.eyePos.z;
	gl_Position = MVP * vec4(position, 1.0f);
}