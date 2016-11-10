#version 450 core
 
layout(location = 0) in vec4 coord;

out vec2 texCoord;

uniform mat4 MVP;
uniform float z = 0.0f;
 
void main()
{
    texCoord = coord.zw;
    gl_Position = MVP * vec4(coord.xy, z, 1.f);
}