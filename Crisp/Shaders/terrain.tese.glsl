#version 450 core

layout(quads, fractional_even_spacing, cw) in;

layout(set = 0, binding = 0) uniform Transforms
{
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

layout(location = 0) out vec3 color;

layout(set = 0, binding = 1) uniform sampler2D heightMap;

void main()
{
    vec4 a = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
    vec4 b = mix(gl_in[3].gl_Position, gl_in[2].gl_Position, gl_TessCoord.x);
    vec4 position = mix(a, b, gl_TessCoord.y);

    float elevation = texture(heightMap, gl_TessCoord.xy * 2.0f).r;
    float range = 0.05f;
    position.y += range * (elevation - 0.5f);

    color = vec3(position.xz / 129.0f, 0.0f);

    gl_Position = MVP * position;
}