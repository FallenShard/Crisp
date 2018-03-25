#version 450 core

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 0) uniform sampler2D tex;

layout(push_constant) uniform PushConstant
{
    layout(offset = 0)  vec2 texelSize;
    layout(offset = 8)  float sigma;
    layout(offset = 12) int radius;
} blur;

const float PI = 3.1415926535897932384626433832795;
const float gaussianFactor = 1.0f / sqrt(2.0f * PI);

void main()
{
    vec2 texelSize = vec2(blur.texelSize.x, blur.texelSize.y);

    float eTerm = -0.5f / (blur.sigma * blur.sigma);
    float fTerm = gaussianFactor / blur.sigma;
    
    vec4 colorSum = vec4(texture(tex, texCoord).rgb * fTerm, fTerm);
    for (int i = 1; i <= blur.radius; i++)
    {
        float w = fTerm * exp(eTerm * i * i);

        colorSum += vec4(texture(tex, texCoord + i * texelSize).rgb * w, w);
        colorSum += vec4(texture(tex, texCoord - i * texelSize).rgb * w, w);
    }

    finalColor = vec4(colorSum.rgb / colorSum.w, 1.0f);
}
