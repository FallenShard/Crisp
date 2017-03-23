#version 450 core

in VsOut
{
	vec3 position;
} fsIn;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 1) uniform CameraParams
{
	mat4 V;
	mat4 P;
	vec2 screenSize;
	vec2 nearFar;
};

layout(set = 0, binding = 2) uniform sampler s;
layout(set = 0, binding = 3) uniform texture2D sceneTex;

void main()
{
	finalColor = vec4(2 * fsIn.position, 1.0f);
}