#version 450 core

layout (location = 0) in vec3 texCoords;

layout (location = 0) out vec4 color;

layout(set = 0, binding = 1) uniform samplerCube skybox;

void main()
{
    vec4 texel = texture(skybox, texCoords);
    color = vec4(pow(texel.rgb, vec3(2.2f)), texel.a);
}