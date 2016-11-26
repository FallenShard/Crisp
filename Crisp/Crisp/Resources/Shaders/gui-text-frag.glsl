#version 450 core
 
layout(location = 0) in vec2 fsTexCoord;

layout(location = 0) out vec4 finalColor;

layout(set = 1, binding = 0) uniform sampler s;
layout(set = 1, binding = 1) uniform texture2D tex;

layout(set = 1, binding = 2) uniform TextColor
{
    vec4 values[16];
} textColor;

layout(push_constant) uniform PushConstant
{
    layout(offset = 4) int value;
} colorIndex;
 
void main()
{
    finalColor = vec4(1.0f, 1.0f, 1.0f, texture(sampler2D(tex, s), fsTexCoord).r) * textColor.values[colorIndex.value];
}