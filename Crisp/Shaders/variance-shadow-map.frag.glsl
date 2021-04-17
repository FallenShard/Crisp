#version 450 core

layout(location = 0) in float eyeZ;

layout(location = 0) out vec2 distances;

void main()
{
    distances = vec2(eyeZ, eyeZ * eyeZ);
}