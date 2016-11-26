#version 450 core
 
layout(location = 0) in vec2 fsTexCoord;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 0) uniform sampler s;
layout(set = 0, binding = 1) uniform texture2DArray texArray;

layout(push_constant) uniform TextureIndex
{
    layout(offset = 0) int value;
} texIndex;
 
void main()
{
    vec3 texCoord;
    texCoord.xy = fsTexCoord;
    texCoord.z = float(texIndex.value);
    finalColor = texture(sampler2DArray(texArray, s), texCoord);
}