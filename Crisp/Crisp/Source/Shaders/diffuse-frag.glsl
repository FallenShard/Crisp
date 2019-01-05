#version 450 core

layout(location = 0) in vec2 fsTexCoord;

layout(location = 0) out vec4 finalColor;

layout(set = 1, binding = 0) uniform sampler2D tex;

void main()
{
    vec4 sam = texture(tex, fsTexCoord);
    finalColor = vec4(pow(sam.rgb, vec3(2.2f)), 1.0f);// texture(tex, fsTexCoord);
}