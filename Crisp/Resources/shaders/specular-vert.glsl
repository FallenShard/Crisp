#version 450 core

void main()
{
    gl_Position = MVP * vec4(position, 1.0f);
}