#version 460 core
#define PI 3.1415926535897932384626433832795

layout(location = 0) in vec3 position;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform samplerCube cubeMap;

vec3 getIrradiance(vec3 normal)
{
    vec3 irradiance = vec3(0.0f);

    vec3 up    = vec3(0.0f, 1.0f, 0.0f);
    vec3 right = cross(up, normal);
    up         = cross(normal, right);

    float sampleDelta = 0.01f;
    float nrSamples = 0.0f; 
    for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta)
    {
        for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta)
        {
            float cosTheta = cos(theta);
            float sinTheta = sin(theta);
            // spherical to cartesian (in tangent space)
            vec3 tangentSample = vec3(sinTheta * cos(phi),  sinTheta * sin(phi), cosTheta);
            // tangent space to world
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal; 

            irradiance += texture(cubeMap, sampleVec).rgb * cosTheta * sinTheta;
            nrSamples++;
        }
    }
    return PI * irradiance * (1.0f / float(nrSamples));
}

void main()
{		
    vec3 color = getIrradiance(normalize(position));
    
    fragColor = vec4(color, 1.0);
}