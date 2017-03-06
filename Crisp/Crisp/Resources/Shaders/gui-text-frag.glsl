#version 450 core
 
layout(location = 0) in vec2 fsTexCoord;

layout(location = 0) out vec4 finalColor;

layout(set = 1, binding = 0) uniform TextColor
{
    vec4 values[16];
} textColor;

layout(set = 2, binding = 0) uniform sampler s;
layout(set = 2, binding = 1) uniform texture2D tex;

layout(push_constant) uniform PushConstant
{
    layout(offset = 4) int value;
} colorIndex;
 
void main()
{
    finalColor = textColor.values[colorIndex.value] * texture(sampler2D(tex, s), fsTexCoord).r;
}