#version 450 core

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 0) uniform sampler2D lightTex;
layout(set = 0, binding = 1) uniform LightShaftParams
{
    vec2 lightPos;
    float exposure;
    float decay;
    float density;
    float weight;
};

const int NumSamples = 100;


void main()
{
    vec2 deltaTexCoord = texCoord - lightPos;
    vec2 currTexCoord = texCoord;
    deltaTexCoord *= 1.0 / float(NumSamples) * density;

    finalColor = vec4(0.0);

    float illuminationDecay = 1.0f;
    for(int i = 0; i < NumSamples ; i++)
    {
        currTexCoord -= deltaTexCoord;
        vec4 texSample = texture(lightTex, currTexCoord);
        texSample *= illuminationDecay * weight;
        finalColor += texSample;
        illuminationDecay *= decay;
    }

    finalColor *= exposure;
}
