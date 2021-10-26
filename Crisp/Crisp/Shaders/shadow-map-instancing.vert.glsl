#version 450 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 col0;
layout(location = 2) in vec4 col1;
layout(location = 3) in vec4 col2;
layout(location = 4) in vec4 col3;

layout(set = 0, binding = 0) uniform LightTransforms
{
    mat4 LVP[4];
};

layout(push_constant) uniform PushConstant
{
	layout(offset = 0) uint value;
} lightTransformIndex;

void main()
{
    mat4 M = mat4(col0, col1, col2, col3);
    gl_Position = LVP[lightTransformIndex.value] * M * vec4(position, 1.0f);
}
