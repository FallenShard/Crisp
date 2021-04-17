#version 450 core

layout (location = 0) in vec3 texCoords;

layout (location = 0) out vec4 color;

layout(set = 0, binding = 1) uniform samplerCube skybox;

void main()
{
    color = vec4(texture(skybox, texCoords).rgb, 1.0f);
}