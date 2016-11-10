#version 450 core
 
smooth in vec2 fsTexCoord;

out vec4 finalColor;

uniform sampler2D tex;

float toSrgb(float value)
{
    if (value < 0.0031308)
        return 12.92 * value;
    return 1.055 * pow(value, 0.41666f) - 0.055;
}
 
void main()
{
    vec4 texel = texture(tex, fsTexCoord);
    finalColor = vec4(toSrgb(texel.r), toSrgb(texel.g), toSrgb(texel.b), 1.0f);
}