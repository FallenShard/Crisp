#version 450 core

layout(location = 0) in vec3 position;

layout(set = 0, binding = 1) uniform sampler2D heightMap;

void main()
{
    vec2 texCoord = position.xz / 129.0f;
    float elevation = texture(heightMap, texCoord).r;
    gl_Position = vec4(vec3(position.x, elevation * 10.0f - 5.0f, position.z), 1.0f);
}