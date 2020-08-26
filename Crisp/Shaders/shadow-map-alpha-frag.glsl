#version 460 core

layout(location = 0) in vec2 outTexCoord;

layout(set = 1, binding = 0) uniform sampler2D tex;

void main()
{
    vec4 colorSample = texture(tex, outTexCoord).rgba;
    if (colorSample.a < 0.1)
        discard;
}