#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec4 tangent;

layout(location = 0) out vec3 vertexData;

void main() {
    vertexData = normal + vec3(texCoord, 0.0) + tangent.xyz;
    gl_Position = vec4(position, 1.0);
}
