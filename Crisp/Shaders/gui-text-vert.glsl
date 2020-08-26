#version 450 core
 
layout(location = 0) in vec4 posCoord;

layout(location = 0) out vec2 fsTexCoord;

layout(set = 0, binding = 0) uniform GuiTransform
{
    mat4 MVP[4];
} guiTransform;

layout(push_constant) uniform PushConstant
{
    layout(offset = 0) int index;
} pushConsts;
 
void main()
{
    fsTexCoord  = posCoord.zw;
    gl_Position = guiTransform.MVP[pushConsts.index] * vec4(posCoord.xy, 0.0f, 1.0f);
}