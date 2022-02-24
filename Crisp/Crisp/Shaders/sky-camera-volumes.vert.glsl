#version 450 core

layout(location = 0) in vec2 position;

layout(location = 0) out flat int sliceId;

void main()
{
    sliceId = gl_InstanceIndex;
    gl_Position = vec4(position, 0.0f, 1.0f);
}