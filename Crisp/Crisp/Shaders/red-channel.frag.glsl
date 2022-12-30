#version 450 core
 
layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 0) uniform sampler2D tex;
 
void main()
{
    finalColor = vec4(texture(tex, texCoord).r, 0.0f, 0.0f, 1.0f);
}