#version 450 core

layout(location = 0) in vec3 inColor;

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

    float cosTheta = max(0, dot(normal, vec3(0, 0, 1)));

    finalColor = vec4(cosTheta * inColor, 1.0f);
}