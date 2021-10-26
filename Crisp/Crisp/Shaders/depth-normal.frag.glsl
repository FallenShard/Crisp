#version 450 core

layout(location = 0) in smooth vec3 eyeNormal;

layout(location = 0) out vec4 normal;

void main()
{
    normal = vec4(vec3(normalize(eyeNormal)), gl_FragCoord.z);
}