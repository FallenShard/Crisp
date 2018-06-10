#version 450 core
 
layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 0) uniform sampler2DArray tex;

layout(push_constant) uniform TextureIndex
{
    layout(offset = 0) int texIndex;
};
 
void main()
{
    vec3 texCoord;
    texCoord.xy = uv;
    texCoord.z  = float(texIndex);
    vec4 color = texture(tex, texCoord);
    finalColor = vec4(color.rgb, color.a);
}