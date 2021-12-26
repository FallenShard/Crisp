#version 460 core

layout(location = 0) in vec2 position;

layout(location = 0) out vec2 texCoord;

void main()
{
    texCoord = position * 0.5f + 0.5f;
    gl_Position = vec4(position, 0.0f, 1.0f);
}