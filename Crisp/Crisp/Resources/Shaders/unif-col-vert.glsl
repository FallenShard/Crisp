#version 450 core

layout(location = 0) in vec3 position;

layout(set = 0, binding = 0) uniform Transforms
{
	mat4 MVP;
	mat4 MV;
	mat4 M;
	mat4 N;
};

out VsOut
{
	vec3 position;
} vsOut;

void main()
{
	vsOut.position = position;
	gl_Position = MVP * vec4(position, 1.0f);
}