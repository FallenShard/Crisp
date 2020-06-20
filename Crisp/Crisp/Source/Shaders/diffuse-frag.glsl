#version 450 core

layout(location = 0) in vec3 eyeNormal;

layout(location = 0) out vec4 finalColor;

layout(push_constant) uniform PushConstant
{
    layout(offset = 0) vec4 color;
};

void main()
{
    vec3 eyeN = normalize(eyeNormal);

    finalColor = vec4(color.rgb, 1.0f);
}