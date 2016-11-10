#version 450 core
 
smooth in vec2 fsTexCoord;

out vec4 finalColor;

uniform sampler2D tex;
 
void main()
{
    finalColor = texture(tex, fsTexCoord);
}