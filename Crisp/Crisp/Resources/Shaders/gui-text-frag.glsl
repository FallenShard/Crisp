#version 450 core
 
in vec2 texCoord;

out vec4 finalColor;

uniform sampler2D atlas;
uniform vec4 color;
 
void main()
{
    finalColor = vec4(1.0f, 1.0f, 1.0f, texture(atlas, texCoord).r) * color;
}