#version 450 core

in VsOut
{
    vec3 eyePos;
} fsIn;

layout(location = 0) out vec4 finalColor;

layout(set = 1, binding = 0) uniform ParticleParams
{
    float radius;
    float screenSpaceScale;
};

void main()
{
    vec3 normal;
    normal.xy = gl_PointCoord * vec2(2.0, -2.0) - vec2(1.0, -1.0);
    float r2 = dot(normal.xy, normal.xy);
    if (r2 > 1.0)
        discard;

    normal.z = sqrt(1.0 - r2);

    finalColor = vec4(normal, 1.0f);
}