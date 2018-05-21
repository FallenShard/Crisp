#version 450 core

layout(location = 0) in vec3 color;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 0) uniform Transforms
{
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

layout(set = 0, binding = 1) uniform sampler2D heightMap;

void main()
{
    float offset = 1.0f / textureSize(heightMap, 0).x;
    const vec3 off = vec3(-offset, 0, offset);

    float heightFactor = 20.0f;

    float hL = texture(heightMap, color.xy + off.xy).r * heightFactor;
    float hR = texture(heightMap, color.xy + off.zy).r * heightFactor;
    float hD = texture(heightMap, color.xy + off.yz).r * heightFactor;
    float hU = texture(heightMap, color.xy + off.yx).r * heightFactor;

    vec3 normal;
    normal.x = hL - hR;
    normal.z = hU - hD;
    normal.y = 2.0f;
    normal = normalize(normal);
    normal = normalize(vec3(N * vec4(normal, 0.0f)));

    vec4 lightDir = vec4(normalize(vec3(1.0f, 1.0f, 1.0f)), 0.0f);
    vec4 eyeLight = MV * lightDir;

    float cosFactor = max(0.0f, dot(normal, eyeLight.xyz));

    finalColor = vec4(vec3(cosFactor), 1.0f);
}