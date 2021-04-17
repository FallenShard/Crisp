#version 450 core
 
layout(location = 0) in vec2 fsTexCoord;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 1) uniform TextColor
{
    vec4 values[16];
} textColor;

layout(set = 1, binding = 0) uniform sampler s;
layout(set = 1, binding = 1) uniform texture2D tex;

layout(push_constant) uniform PushConstant
{
    layout(offset = 4) int value;
} colorIndex;
 
void main()
{
    vec4 color = textColor.values[colorIndex.value];
    float alpha = color.a * texture(sampler2D(tex, s), fsTexCoord).r;
    finalColor = vec4(color.r, color.g, color.b, 1.0f) * alpha;
}