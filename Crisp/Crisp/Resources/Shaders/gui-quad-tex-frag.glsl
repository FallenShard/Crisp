#version 450 core
 
smooth in vec2 fsTexCoord;

uniform sampler2D tex;

uniform vec2 texCoordScale = vec2(1.0f, 1.0f);
uniform vec2 texCoordOffset = vec2(0.0f, 0.0f);
uniform vec4 color;

out vec4 finalColor;
 
void main()
{
    finalColor = texture(tex, fsTexCoord * texCoordScale + texCoordOffset) * color;
}