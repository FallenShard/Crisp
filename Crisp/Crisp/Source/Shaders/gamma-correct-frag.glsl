#version 450 core
 
layout(location = 0) in vec2 fsTexCoord;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 0) uniform sampler2DArray tex;

layout(push_constant) uniform TextureIndex
{
    layout(offset = 0) int texIndex;
};

float toSrgb(float value)
{
    if (value < 0.0031308)
        return 12.92 * value;
    return 1.055 * pow(value, 0.41666f) - 0.055;
}
 
void main()
{
    vec3 texCoord = vec3(fsTexCoord, float(texIndex));
    vec4 texel = texture(tex, texCoord);
    finalColor = vec4(toSrgb(texel.r), toSrgb(texel.g), toSrgb(texel.b), texel.a);
}