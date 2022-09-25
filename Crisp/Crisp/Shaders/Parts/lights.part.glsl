
// General structure, different types will use different fields
struct Light
{
    mat4 V;
    mat4 P;
    mat4 VP;
    vec4 position;
    vec4 direction;
    vec4 spectrum;
    vec4 params;
};

// Assume there's a global camera view matrix V

// ----- Spot Light
vec3 evalSpotLightRadiance(const in Light light, const in mat4 viewMatrix)
{
    vec3 eyeO = (viewMatrix * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
    vec3 eyeL = (viewMatrix * vec4(light.position.xyz, 1.0f)).xyz;
    vec3 coneDir = eyeO - eyeL;

    vec3 eyeP = eyePosition;
    vec3 lightToPos = eyeP - eyeL;

    float sim = dot(normalize(coneDir), normalize(lightToPos));
    if (sim < 0.7f)
        return vec3(0.0f);
    else if (sim < 0.8f)
        return vec3(smoothstep(0.7, 0.8, sim));//vec3((sim - 0.7f) / 0.1f);
    else
        return vec3(1.0f);
}

// ----- Point Light
vec3 evalPointLightRadiance(const in Light light, const in mat4 viewMatrix, out vec3 eyeL)
{
    const vec3 eyeLightPos = (viewMatrix * vec4(light.position.xyz, 1.0f)).xyz;
    const vec3 eyePosToLight = eyeLightPos - eyePosition;
    const float dist = length(eyePosToLight);

    eyeL = eyePosToLight / dist;
    return light.spectrum / (dist * dist);
}

// ----- Point Light
vec3 evalPointLightRadiance(const in mat4 viewMatrix, vec3 lightPos, vec3 spectrum, out vec3 eyeL)
{
    const vec3 eyeLightPos = (viewMatrix * vec4(lightPos, 1.0f)).xyz;
    const vec3 eyePosToLight = eyeLightPos - eyePosition;
    const float dist = length(eyePosToLight);

    eyeL = eyePosToLight / dist;
    return spectrum / (dist * dist);
}

// ----- Directional Light
vec3 evalDirectionalLightRadiance(in Light light, const in mat4 viewMatrix, out vec3 eyeL)
{
    eyeL = normalize((viewMatrix * light.direction).xyz);
    return light.spectrum;
}

// ----- Environment Light
vec3 computeEnvLightRadiance(
    in samplerCube diffuseIrradianceMap,
    in samplerCube specularIrradianceMap,
    in sampler2D brdfLut,
    const in mat4 viewMatrix, 
    vec3 eyeN, vec3 eyeV, vec3 kD, vec3 albedo, vec3 F, float roughness, float ao)
{
    const mat4 invV = inverse(viewMatrix);
    const vec3 worldN = (invV * vec4(eyeN, 0.0f)).rgb;
    const vec3 irradiance = texture(diffuseIrradianceMap, worldN).rgb;
    const vec3 diffuse = irradiance * albedo;

    const float NdotV = max(dot(eyeN, eyeV), 0.0f);
    const vec3 eyeR = reflect(-eyeV, eyeN);
    const vec3 worldR = (invV * vec4(eyeR, 0.0f)).rgb;

    const float MaxReflectionLod = 4.0f;
    const vec3 prefilter = textureLod(specularIrradianceMap, worldR, roughness * MaxReflectionLod).rgb;
    const vec2 brdf = texture(brdfLut, vec2(NdotV, roughness)).xy;
    const vec3 specular = prefilter * (F * brdf.x + brdf.y);

    return (kD * diffuse + specular) * ao;
}