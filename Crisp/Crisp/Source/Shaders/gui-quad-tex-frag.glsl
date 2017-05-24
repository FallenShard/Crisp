#version 450 core
 
layout(location = 0) in vec2 fsTexCoord;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 1) uniform TextColor
{
    vec4 values[16];
} colors;

layout(set = 1, binding = 0) uniform sampler2D tex;
layout(set = 1, binding = 1) uniform TexCoordTransform
{
    vec4 values[16];
} texCoordTransform;

layout(push_constant) uniform PushConstant
{
    layout(offset = 4) int colorIndex;
    layout(offset = 8) int tctIndex;
} pushConst;
 
void main()
{
    vec4 texCoordOffset = texCoordTransform.values[pushConst.tctIndex];
    vec2 finalTexCoord = fsTexCoord * texCoordOffset.xy + texCoordOffset.zw;

    vec4 color = colors.values[pushConst.colorIndex];
    vec4 texSample = texture(tex, finalTexCoord);
    float alpha = color.a * texSample.a;
    finalColor = vec4(color.rgb * texSample.rgb, 1.0f) * alpha;
}