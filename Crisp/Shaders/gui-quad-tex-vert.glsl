#version 450 core
 
layout(location = 0) in vec2 position;

layout(location = 0) out vec2 fsTexCoord;

layout(set = 0, binding = 0) uniform GuiTransform
{
    mat4 MVP[4];
} guiTransform;

layout(push_constant) uniform PushConstant
{
    layout(offset = 0) int value;
} transformIndex;
 
void main()
{
    fsTexCoord  = position;
    gl_Position = guiTransform.MVP[transformIndex.value] * vec4(position, 0.0f, 1.0f);
}