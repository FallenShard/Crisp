
const vec3 NdcMin = vec3(-1.0f, -1.0f, 0.0f);
const vec3 NdcMax = vec3(+1.0f, +1.0f, 1.0f);

// ----- Standard Shadow Mapping
float getShadowCoeff(float bias, mat4 lightVP)
{
    vec4 lightSpacePos = lightVP * vec4(worldPos, 1.0f);
    vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;

    if (any(lessThan(ndcPos, NdcMin)) || any(greaterThan(ndcPos, NdcMax)))
        return 0.0f;

    vec2 texCoord = ndcPos.xy * 0.5f + 0.5f;

    ivec2 size = textureSize(shadowMap, 0).xy;
    vec2 texelSize = vec2(1) / size;

    float shadowMapDepth = texture(pointShadowMap, tc).r;
    return shadowMapDepth < (lightSpacePos.z - bias) / lightSpacePos.w ? 0.0f : 1.0f;
}

// ----- Percentage Closer Filtering
float getPcfShadowCoeff(float bias, mat4 lightVP, int pcfRadius)
{
    vec4 lightSpacePos = lightVP * vec4(worldPos, 1.0f);
    vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;

    if (any(lessThan(ndcPos, NdcMin)) || any(greaterThan(ndcPos, NdcMax)))
        return 0.0f;

    vec2 texCoord = ndcPos.xy * 0.5f + 0.5f;

    ivec2 size = textureSize(shadowMap, 0).xy;
    vec2 texelSize = vec2(1) / size;

    const float numSamples = (2 * pcfRadius + 1) * (2 * pcfRadius + 1);

    float amount  = 0.0f;
    for (int i = -pcfRadius; i <= pcfRadius; i++)
    {
        for (int j = -pcfRadius; j <= pcfRadius; j++)
        {
            vec2 tc = texCoord + vec2(i, j) * texelSize;
            float shadowMapDepth = texture(pointShadowMap, tc).r;
            amount += shadowMapDepth < (lightSpacePos.z - bias) / lightSpacePos.w ? 0.0f : 1.0f;
        }
    }

    return amount / numSamples;
}

// ----- Cascaded Shadow Mapping
bool isInCascade(vec3 worldPos, mat4 lightVP)
{
    vec4 lightSpacePos = lightVP * vec4(worldPos, 1.0f);
    vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;

    return all(greaterThan(ndcPos, NdcMin)) && all(lessThan(ndcPos, NdcMax));
}

// Check-in-bounds based
float getCsmShadowCoeffDebug(float cosTheta, out vec3 color)
{
    color = vec3(0.0f, 0.0f, 1.0f);
    int cascadeIndex = 3;
    if (isInCascade(0))
    {
        color = vec3(1.0f, 0.0f, 0.0f);
        cascadeIndex = 0;
    }
    else if (isInCascade(1))
    {
        color = vec3(1.0f, 1.0f, 0.0f);
        cascadeIndex = 1;
    }
    else if (isInCascade(2))
    {
        color = vec3(0.0f, 1.0f, 0.0f);
        cascadeIndex = 2;
    }

    color *= 0.3f;

    float bias = 0.005 * tan(acos(cosTheta)); // cosTheta is dot( n,l ), clamped between 0 and 1
    bias = clamp(bias, 0.0f, 0.01f);
    return getShadowCoeff(bias, lightTransforms.VP[cascadeIndex], cascadeIndex);
}



// ----- Variance Shadow Mapping
vec3 getVsmCoeff(vec3 worldPos, mat4 lightVP, float bias)
{
    vec4 lightSpacePos = lightVP * vec4(worldPos, 1.0f);
    vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;

    if (any(lessThan(ndcPos, NdcMin)) || any(greaterThan(ndcPos, NdcMax)))
        return vec3(0.0f);

    vec2 texCoord = ndcPos.xy * 0.5f + 0.5f;

    ivec2 size = textureSize(pointShadowMap, 0).xy;
    vec2 texelSize = vec2(1.0f) / size;

    int pcfRadius = 5;
    vec2 moments = vec2(0.0f);
    for (int i = -pcfRadius; i <= pcfRadius; i++)
    {
        for (int j = -pcfRadius; j <= pcfRadius; j++)
        {
            vec2 tc = texCoord + vec2(i, j) * texelSize;
            moments += texture(pointShadowMap, tc).rg;
        }
    }

    moments /= (2 * pcfRadius + 1) * (2 * pcfRadius + 1);

    // TODO
    vec4 lvPos = light.V * vec4(worldPos, 1.0f);
    float fragDepth = -lvPos.z;
    float E_x2 = moments.y;
    float Ex_2 = moments.x * moments.x;

    float variance = E_x2 - Ex_2;
    float mD = fragDepth - moments.x;
    float mD_2 = mD * mD;
    float p = variance / (variance + mD_2);
    float lit = max( p, float(fragDepth <= moments.x) );
    return vec3(lit);
}