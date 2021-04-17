#version 460 core

layout(push_constant) uniform PushConstant
{
    layout(offset = 0) vec4 positions[8];
    layout(offset = 128) mat4 MVP;
};

void main()
{
    gl_Position = MVP * positions[gl_VertexIndex];
}