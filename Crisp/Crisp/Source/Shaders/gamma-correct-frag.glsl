#version 450 core
 
layout(location = 0) in vec2 fsTexCoord;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 0) uniform sampler2D tex;

float toSrgb(float value)
{
    if (value < 0.0031308)
        return 12.92 * value;
    return 1.055 * pow(value, 0.41666f) - 0.055;
}
 
void main()
{
    vec4 texel = texture(tex, fsTexCoord);
    finalColor = vec4(toSrgb(texel.r), toSrgb(texel.g), toSrgb(texel.b), texel.a);
}