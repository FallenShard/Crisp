#version 450 core

in VsOut
{
	vec3 eyePos;
} fsIn;

layout(location = 0) out vec4 finalColor;

layout(set = 1, binding = 0) uniform ParticleParams
{
	float radius;
	float screenSpaceScale;
};

void main()
{
	finalColor = vec4(1.0f, 0.0f, 0.5f, 1.0f);
}